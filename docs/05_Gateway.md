# Gateways (南北向介面對接與防腐層)

## 系統定位 (Verified Narrative)
本子系統實作為 IT 邊緣層的「南北向外交官」與嚴格的**防腐層（Anti-Corruption Layer, ACL）**。
它包含了南向（`v5_ot_gateway`）與北向（`v5_it_gateway`）兩個獨立節點。其核心任務是將底層 Linux Kernel 的實體世界，以及不可靠的外部雲端網路（Cloud/Firebase）進行雙向隔離。透過吸收所有 I/O 延遲、型別錯亂與斷線風險，它為核心的大腦（Core Runtime）創造了一個純淨、無延遲且無型別錯誤的決策環境。

---

## 1. Architecture (架構意圖與邊界)

### 存在目的 (Purpose)
建立強大的 I/O 與網路物理防護邊界，徹底隔絕硬體失效（如 File Descriptor 異常）與外部網路抖動（如 Wi-Fi 斷線、JSON 格式損毀），確保核心「領域策略引擎（FSM）」絕對不會因等待外部資源而發生系統阻塞。

### 互動關係 (Relationships)
本系統展現了極致的外部依賴解耦：
*   **南向解耦 (Southbound)：** `ot_gateway` 是全系統**唯一**知道 `v5_ioctl_contract.h` 存在的 IT 節點。它吸納了所有 C Struct 的解析複雜度，對北向只釋放乾淨標準的 `OTState` Topic。
*   **北向解耦 (Northbound)：** `it_gateway` 是全系統**唯一**知道外部網路 SDK（如 Firebase）存在的節點。Core Bridge 完全不需要安裝任何網路套件即可獨立運作。

### ⛔ 絕對邊界 (Architecture Boundaries)
為維持防腐層的純度，Gateways 嚴格遵守以下禁區：
*   **零領域邏輯 (Zero Domain Logic)：** 貫徹「只負責搬運與翻譯，不負責做決定」的單一職責原則。Gateway 內部**絕對不包含**任何類似 `if (pm25 > 100) then alarm` 的業務判斷。
*   **執行緒絕對隔離 (Thread Isolation)：** 外部網路通訊（如 HTTPS I/O）必須封裝於獨立的 Daemon 執行緒中，**絕對不允許**在 ROS 2 的主 Spin 迴圈中進行阻塞式網路呼叫。

---

## 2. Implementation (實作機制與客觀證據)

本模組的防禦能力，均具備以下程式碼層級的動態與靜態證據支撐：

*   **優雅降級與硬體輪詢 (OT Gateway - C++)：**
    負責定時（100ms）開啟並輪詢 `/dev/v5_safety_core`（透過 `open` 與 `ioctl`）。當偵測到硬體失效或未連接（`if (fd_ < 0)`）時，具備動態降級能力，自動派發安全的「虛擬模擬模式」數據，確保系統不會因底層中斷而 Crash。
*   **非同步通訊與流量塑形 (IT Gateway - Python)：**
    將緩慢的 Firebase 網路寫入操作（`self.db_ref.set`）完全隔離在獨立的 Daemon 執行緒（`transmit_worker`）中。並利用優先權佇列（`queue.PriorityQueue(maxsize=100)`），確保緊急狀態指令（`prio = 0`）能插隊上傳，不被一般遙測數據（Telemetry）阻塞。
*   **下行型別防禦 (Type Interception)：**
    實作了強硬的動態行為攔截。在收到雲端指令時，執行嚴格的格式校驗（如 `if not isinstance(..., (float, int)): return`），在邊界直接過濾並丟棄惡意的 `ILLEGAL_STRING` 等污染源，再打包成安全的內部 `CloudIntent`。

---

## 3. Engineering Assessment (工程限制與設計取捨)

> **架構取捨決策 (Architecture Decisions & Trade-offs)**

*   **Decision (決策):** 引入防腐層，將 I/O 與網路通訊從 Core Node 剝離至獨立的 Gateway Nodes。
*   **Constraint (限制):** 系統資料流路徑變長，所有進出邊緣控制器的訊號，都必須多經歷一次 ROS 2 Topic 的發布/訂閱（Pub/Sub）延遲與序列化消耗。
*   **Trade-off (取捨):** 在 Mixed Criticality System 中，我們自願犧牲微秒級的系統內部傳輸效能，來換取 **「核心狀態機的絕對不阻塞」**與 **「型別安全的防護縱深」**。這是一個確保系統在惡劣網路或硬體環境下仍能維持強韌（Resilient）的必要工程代價。

---

## 4. Future Evolution (未來演進方向)

基於現有 Gateway 作為 I/O 與網路邊界的定位，未來的架構演進方向將評估：
1.  **離線資料緩衝機制 (Offline Telemetry Buffer)：** 針對 `it_gateway` 擴充本地端 SQLite 或 Redis 緩衝。當發生長時間 Wi-Fi 斷線導致 Priority Queue 滿載（`>100`）時，能將一般遙測數據寫入本地儲存，待網路恢復後再非同步回補，以實現完整的斷線重連機制。
2.  **動態輪詢頻率 (Adaptive Polling)：** 針對 `ot_gateway` 評估引入動態週期。當系統處於 `STATE_EMERGENCY` 時，自動將南向 IOCTL 輪詢頻率從 100ms 提高至 10ms，以獲取更高解析度的物理快照。