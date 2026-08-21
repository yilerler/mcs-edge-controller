# 05_Gateway.md

## 1. Identity & Purpose (身分與目的)

* **Module Name:** `gateways` (`v5_ot_gateway` 與 `v5_it_gateway`)
* **Architecture Level:** `Level 2 (Anti-Corruption Layer & North-South Adapters)`
* **Role:** 南北向介面對接與防腐層 (Anti-Corruption Layer, ACL)。
* **Core Responsibility:** 將底層 Linux Kernel 的實體資料與外部雲端網路進行雙向隔離，吸收 I/O 延遲、型別異常與連線風險，為核心策略引擎提供無阻塞的資料交換通道。

## 2. Architecture Boundary (架構邊界)

本層級作為防腐層，由兩個獨立的適配器（Adapters）組成，將外部的不確定性完全阻絕於 ROS 2 DDS 匯流排之外：

* **北向雲端閘道器 (`v5_it_gateway`):**
	* **下行 (Downlink):** 接收 Firebase 雲端動態 JSON。**邊界約束：型別防禦屏障。** 直接攔截並丟棄所有不合法的型別，轉譯為純淨的 `CloudIntent.msg` 發布至 DDS。
	* **上行 (Uplink):** 訂閱內部 `SafetyState.msg`。**邊界約束：非同步流量塑形。** 將網路 I/O 封裝於獨立執行緒，並透過優先權佇列保障緊急警報的頻寬。


* **南向實體閘道器 (`v5_ot_gateway`):**
	* **上行 (Uplink):** 透過 `/dev/v5_safety_core` 輪詢 12 Bytes ABI。**邊界約束：硬體優雅降級。** 當硬體損毀 (`fd < 0`) 時，主動派發保底數據，轉譯為 `OTState.msg` 發布至 DDS。


* **Strict Constraints (嚴格限制):**
	* **零領域邏輯 (Zero Domain Logic):** 閘道器內部嚴禁包含任何業務判斷（如 `if (pm25 > 100)`），僅負責通訊協定轉換與資料清理。
	* **執行緒絕對隔離 (Thread Isolation):** 外部網路通訊與 Kernel I/O 必須封裝，嚴禁在 ROS 2 的主 Spin 迴圈中進行阻塞式 (Blocking) 系統呼叫。



### 📊 Canonical Diagram: Anti-Corruption Layer (ACL) Boundaries

本圖表呈現 Level 2 的南北向防腐邊界。視覺化了 Gateway 如何吸收外部環境（WAN 與 Kernel）的毒性，保護內部 DDS 匯流排與核心大腦的純淨性（參見 Section 3）。

```text
  ======================= WAN / CLOUD (External IT) =======================
          │ (Downlink: JSON)                             ▲ (Uplink: JSON)
          ▼                                              │
  +-----------------------------------------------------------------------+
  | L2: 北向防腐層 [v5_it_gateway] (Python)                                 |
  |                                                                       |
  |   [ TYPE SHIELD ] (型別攔截)               [ DAEMON THREAD ] (非同步 I/O)|
  |   (Drop if invalid type)                  (Non-blocking WAN Tx)       |
  |          │                                           ▲                |
  |          ▼                                           │                |
  |  [ CloudIntent.msg ]                       [ PRIORITY QUEUE ]         |
  |          │                                 (Emergency Prio=0)         |
  +----------│-------------------------------------------▲----------------+
             ▼                                           │
  ~~~~~~~~~~~│~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~│~~~~~~~~~~~~~~~~~
             │              ROS 2 DDS 內部匯流排            │
  ~~~~~~~~~~~│~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~│~~~~~~~~~~~~~~~~~
             │                                           │
    (To L1 Core Bridge)                        (From L1 Core Bridge)
                                                      
             ▲ (OTState.msg)
             │
  +----------│------------------------------------------------------------+
  | L2: 南向防腐層 [v5_ot_gateway] (C++)                                    |
  |          │                                                            |
  |  [ FALLBACK GENERATOR ] (優雅降級)                                      |
  |  (If fd < 0 -> Dispatch Mock Data)                                    |
  |          ▲                                                            |
  |          │ (100Hz I/O)                                                |
  |    [ IOCTL THREAD ]                                                   |
  +----------▲------------------------------------------------------------+
             │
             │ (12 Bytes ABI Snapshot)
             ▼
  ======================== KERNEL / OT (External OT) ======================

```

> **Visual Semantics (視覺語意與證據綁定)**
> * `[ TYPE SHIELD ]`: 動態型別攔截器。對應 `Item 3`，由 `it_gateway_node.py` 中的 `if not isinstance(...) return` 提供證據。
> * `[ PRIORITY QUEUE ]` & `[ DAEMON THREAD ]`: 網路隔離與流量塑形。對應 `Item 2`，由實作層中的 `queue.PriorityQueue` 與 `daemon=True` 執行緒提供證據。
> * `[ FALLBACK GENERATOR ]`: 硬體抽象與保底派發。對應 `Item 1`，由 `ot_gateway_node.cpp` 中偵測 `fd_ < 0` 自動派發安全預設值的邏輯提供證據。
> 
> 

---

## 3. Validation Traceability (驗證溯源)

* **Item 1: 硬體抽象與優雅降級 (Hardware Abstraction & Graceful Degradation)**
	* **Evidence (客觀證據):** [`ot_gateway_node.cpp`](../../it_edge_layer/ros2_ws/src/gateways/v5_ot_gateway/src/ot_gateway_node.cpp) 以 100ms 週期執行 `open` 與 `ioctl`。當偵測到 `fd_ < 0`（硬體未就緒或損毀）時，切換派發預設的保底數據（如 `V5_FENCE_CLEAR`）。
	* **Architecture Inference (架構推導):** 將硬體的可用性與系統的穩定性脫鉤。即使底層 Kernel 驅動異常，也不會導致 ROS 2 網域發生 Segmentation Fault 或節點崩潰，確保核心大腦能持續接收安全的保底資料並維持運作。 Ref: [`ADR-0008`](../ADR/ADR-0008-Introduce_Anti_Corruption_Layer.md)


* **Item 2: 非同步通訊與流量塑形 (Asynchronous I/O & Traffic Shaping)**
	* **Evidence (客觀證據):** [`it_gateway_node.py`](../../it_edge_layer/ros2_ws/src/gateways/v5_it_gateway/v5_it_gateway/it_gateway_node.py) 將緩慢的 `self.db_ref.set` 網路寫入封裝於 `daemon=True` 的獨立 `transmit_worker` 執行緒，並透過 `queue.PriorityQueue(maxsize=100)` 優先處理 `prio = 0` 的緊急狀態。
	* **Architecture Inference (架構推導):** 物理隔離了廣域網路 (WAN) 的不確定性。透過執行緒分離與優先權佇列，確保緩慢或斷線的雲端連線絕對不會阻塞邊緣端內部的 DDS 訊息發布迴圈，同時保障緊急狀態的無延遲上傳。 Ref: [`ADR-0008`](../ADR/ADR-0008-Introduce_Anti_Corruption_Layer.md), [`ADR-0009`](../ADR/ADR-0009-Gateway_Priority_Queue.md)


* **Item 3: 下行型別防禦 (Downlink Type Interception)**
	* **Evidence (客觀證據):** [`it_gateway_node.py`](../../it_edge_layer/ros2_ws/src/gateways/v5_it_gateway/v5_it_gateway/it_gateway_node.py) 在處理下行指令時，實作了明確的型別檢查，如 `if not isinstance(..., (float, int)): return`。
	* **Architecture Inference (架構推導):** 建立從動態語言 (Python/JSON) 到靜態型別系統 (C++/ROS 2) 的防衛邊界。直接在閘道器攔截不合法的負載（如惡意字串注入），避免異常的「髒資料」污染核心大腦的記憶體空間。 Ref: [`ADR-0010`](../ADR/ADR-0010-Strict_Type_Checking_At_Boundary.md)



## 4. Future Evolution & Triggers (未來演進與觸發條件)

* **Evolution Item 1: 實作離線資料緩衝機制 (Offline Telemetry Buffer)**
	* **Trigger:** 當邊緣設備部署於網路不穩定的案場，常態性發生長時 Wi-Fi 斷線，導致 Priority Queue 滿載（`>100`）並開始丟棄歷史遙測數據時。
	* **Expected Impact (預期影響):** 能達成資料零遺失 (Zero Data Loss) 的斷線重連回補；但將引入本地端 SQLite 或 Redis 的相依性，增加邊緣設備的 Disk I/O 負載與儲存空間需求。


* **Evolution Item 2: 動態南向輪詢頻率 (Adaptive Southbound Polling)**
	* **Trigger:** 當系統處於 `STATE_EMERGENCY` 狀態，且災難應變模組要求物理感測解析度必須小於 10ms 時。
	* **Expected Impact (預期影響):** 提升危機時的物理反應速度；但將顯著增加 Kernel Context Switch 開銷與 CPU 佔用率，並可能影響同一主機上其他非關鍵 IT 服務的運算資源。


