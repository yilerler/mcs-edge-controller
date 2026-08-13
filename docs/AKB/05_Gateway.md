# 05_Gateway.md

## 1. Identity & Purpose (身分與目的)

* **Module Name:** `gateways` (`v5_ot_gateway` 與 `v5_it_gateway`)
* **Architecture Level:** `Level 2 (Anti-Corruption Layer & North-South Adapters)`
* **Role:** 南北向介面對接與防腐層 (Anti-Corruption Layer, ACL)。
* **Core Responsibility:** 將底層 Linux Kernel 的實體資料與外部雲端網路進行雙向隔離，吸收 I/O 延遲、型別異常與連線風險，為核心策略引擎提供無阻塞的資料交換通道。

## 2. Architecture Boundary (架構邊界)

* **Inbound (接收):**
* **南向 (OT):** 透過 `/dev/v5_safety_core` 讀取的 12 Bytes 實體狀態快照。
* **北向 (IT):** 來自雲端平台 (如 Firebase) 的非同步、動態型別 (JSON) 配置意圖。


* **Outbound (輸出):**
* **南向 (OT):** 轉譯為 ROS 2 標準的 `OTState.msg` 發布至網域內。
* **北向 (IT):** 將 `SafetyState.msg` 序列化並上傳至雲端資料庫。


* **Strict Constraints (嚴格限制):**
* **零領域邏輯 (Zero Domain Logic):** 貫徹單一職責原則。Gateway 內部禁止包含任何業務判斷（如 `if (pm25 > 100)`），僅負責通訊協定轉換與資料搬運。
* **執行緒絕對隔離 (Thread Isolation):** 外部網路通訊（如 HTTPS I/O）必須封裝於獨立的執行緒中，禁止在 ROS 2 的主 Spin 迴圈中進行阻塞式 (Blocking) 系統呼叫。



## 3. Validation Traceability (驗證溯源)

* **Item 1: 硬體抽象與優雅降級 (Hardware Abstraction & Graceful Degradation)**
* **Evidence (客觀證據):** [`ot_gateway_node.cpp`](../../it_edge_layer/ros2_ws/src/gateways/v5_ot_gateway/src/ot_gateway_node.cpp) 以 100ms 週期執行 `open` 與 `ioctl`。當偵測到 `fd_ < 0`（硬體未就緒或損毀）時，切換派發預設的保底數據（如 `V5_FENCE_CLEAR`）。
* **Architecture Inference (架構推導):** 將硬體的可用性與系統的穩定性脫鉤。即使底層 Kernel 驅動異常，也不會導致 ROS 2 網域發生 Segmentation Fault 或節點崩潰，確保核心大腦能持續運作。 Ref: [`ADR-0008`](../ADR/ADR-0008-Introduce_Anti_Corruption_Layer.md)


* **Item 2: 非同步通訊與流量塑形 (Asynchronous I/O & Traffic Shaping)**
* **Evidence (客觀證據):** [`it_gateway_node.py`](../../it_edge_layer/ros2_ws/src/gateways/v5_it_gateway/v5_it_gateway/it_gateway_node.py) 將 `self.db_ref.set` 封裝於 `daemon=True` 的獨立 `transmit_worker` 執行緒，並透過 `queue.PriorityQueue(maxsize=100)` 優先處理 `prio = 0` 的緊急狀態。
* **Architecture Inference (架構推導):** 隔離了廣域網路 (WAN) 的不確定性。透過執行緒分離與優先權佇列，確保緩慢或斷線的雲端連線不會阻塞邊緣端內部的 DDS 訊息發布，保障緊急狀態的即時傳輸。 Ref: [`ADR-0008`](../ADR/ADR-0008-Introduce_Anti_Corruption_Layer.md), [`ADR-0009`](../ADR/ADR-0009-Gateway_Priority_Queue.md)


* **Item 3: 下行型別防禦 (Downlink Type Interception)**
* **Evidence (客觀證據):** [`it_gateway_node.py`](../../it_edge_layer/ros2_ws/src/gateways/v5_it_gateway/v5_it_gateway/it_gateway_node.py) 在處理下行指令時，實作了明確的型別檢查，如 `if not isinstance(..., (float, int)): return`。
* **Architecture Inference (架構推導):** 建立從動態語言 (Python/JSON) 到靜態型別系統 (C++/ROS 2) 的邊界過濾器。直接在閘道器攔截不合法的負載（如惡意字串注入），避免異常資料污染核心大腦的記憶體空間。 Ref: [`ADR-0010`](../ADR/ADR-0010-Strict_Type_Checking_At_Boundary.md)



## 4. Future Evolution & Triggers (未來演進與觸發條件)

* **Evolution Item 1: 實作離線資料緩衝機制 (Offline Telemetry Buffer)**
* **Trigger:** 當邊緣設備部署於網路不穩定的案場，常態性發生長時 Wi-Fi 斷線，導致 Priority Queue 滿載（`>100`）並開始丟棄歷史遙測數據時。
* **Expected Impact (預期影響):** 能達成資料零遺失 (Zero Data Loss) 的斷線重連回補；但將引入本地端 SQLite 或 Redis 的相依性，增加邊緣設備的 Disk I/O 負載與儲存空間需求。


* **Evolution Item 2: 動態南向輪詢頻率 (Adaptive Southbound Polling)**
* **Trigger:** 當系統處於 `STATE_EMERGENCY` 狀態，且災難應變模組要求物理感測解析度必須小於 10ms 時。
* **Expected Impact (預期影響):** 提升危機時的物理反應速度；但將顯著增加 Kernel Context Switch 開銷與 CPU 佔用率，並可能影響同一主機上其他非關鍵 IT 服務的運算資源。


