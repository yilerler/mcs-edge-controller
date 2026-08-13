# ADR-0008: 引入防腐層 (ACL) 剝離 I/O 與網路通訊

* **ID:** ADR-0008
* **Date:** 2026-08-01
* **Status:** Accepted
* **Custodian:** `gateways`

---

## 1. Context / Problem (背景與問題)
系統面臨著高度不可靠的外部依賴：底層的 Kernel File Descriptor 可能因硬體故障而讀取異常，北向的廣域網路（Firebase 雲端）更充斥著高延遲、斷線與抖動。如果將這些 I/O 讀取與網路通訊邏輯直接寫在核心決策引擎（Core Bridge）中，任何一個外部資源的阻塞（Blocking）都會導致整個系統大腦停擺，引發災難性的假死狀態。

## 2. Decision (決策內容)
引入防腐層（Anti-Corruption Layer, ACL）架構，建立獨立的 Gateway 節點（`v5_ot_gateway` 與 `v5_it_gateway`），專職處理所有的硬體 I/O 輪詢與外部網路通訊，將其從 Core Bridge 中徹底剝離。

## 3. Rationale (決策理據)
貫徹單一職責與實體隔離原則。將外部依賴的異常風險限縮在「邊界節點」，確保核心狀態機（FSM）能在一個純淨、無延遲、無阻塞的環境中，專注執行微秒級的邏輯推演與決策。

## 4. Alternatives (替代方案)
* **Alternative A:** 將 IOCTL 讀取與 Firebase 上傳邏輯直接寫入 `v5_core_bridge` 節點中，以減少 ROS 2 內部的網路通訊中繼 (Hop) 與延遲。

## 5. Rejected Alternatives (拒絕原因)
* **Rejected Alternative A because:** 嚴重違反關注點分離。外部網路的連線超時 (Timeout) 或硬體驅動的失效會直接卡死 FSM 的 Tick Timer 執行緒，導致整個智慧工地系統喪失應變能力。

## 6. Consequences (預期影響與取捨)
* **Positive:** 達成了優雅降級（Graceful Degradation）。即使底層硬體損毀（`fd_ < 0`）或外部網路斷線，Gateway 都能在邊界吸收這些錯誤並派發保底數據，確保系統核心不會發生崩潰。
* **Negative:** 增加了系統內部資料流的長度。所有進出邊緣控制器的訊號，都必須額外承受一次 ROS 2 Topic 發布/訂閱的序列化消耗與傳輸延遲。

## 7. Related AKB (關聯架構知識庫)
* [`AKB / 05_Gateway.md`](../AKB/05_Gateway.md)
* [`AKB / 04_Core_Runtime.md`](../AKB/04_Core_Runtime.md)
* **Concept:** Anti-Corruption Layer (防腐層)、Hardware Abstraction (硬體抽象)

## 8. Evidence / References (證據與參考資料)
* 實作證據：[`ot_gateway_node.cpp`](../../it_edge_layer/ros2_ws/src/gateways/v5_ot_gateway/src/ot_gateway_node.cpp) 定期執行 `ioctl`，並在偵測到 `fd_ < 0` 時，自動切換派發安全的「虛擬模擬模式」數據。
* 實作證據：[`it_gateway_node.py`](../../it_edge_layer/ros2_ws/src/gateways/v5_it_gateway/v5_it_gateway/it_gateway_node.py) 將緩慢的 `self.db_ref.set` 網路寫入操作完全隔離在 `daemon=True` 的獨立執行緒中。