# 06_Service.md

## 1. Identity & Purpose (身分與目的)

* **Module Name:** `services` (`m3_access_control`, `m4_environment_monitor`, `m5_air_quality_monitor`, `m6_local_display`)
* **Architecture Level:** `Level 3 (Stateless Edge Microservices & Applications)`
* **Role:** IT 邊緣層的周邊感測器、致動器模擬與使用者介面。
* **Core Responsibility:** 承載與核心決策無關的周邊與應用邏輯，包含環境感測訊號數位化、本地端戰情面板顯示，以及執行期的系統壓力驗證。

## 2. Architecture Boundary (架構邊界)

* **Inbound (接收):** 實體感測器訊號 (目前為模擬產生)、來自 `v5_core_bridge` 的 `SafetyState.msg` (M3/M6 訂閱) 與 `OTState.msg` (M6 訂閱)。
* **Outbound (輸出):** 轉換後的 `SafetyState.msg` (M4/M5 發布)、門禁請求字串 `access/door_request` (M3 發布)。
* **Strict Constraints (嚴格限制):**
* **零決策權越界 (Zero Decision Power):** 感測節點不進行業務判斷與狀態修改。發布的 `system_state` 必須鎖定為靜態常數 (如 `STATE_NORMAL`)，決策權 100% 委派給中央大腦 (Core Bridge)。
* **零水平耦合 (Zero Horizontal Coupling):** 微服務彼此之間互不知曉，禁止直接跨服務訂閱 (如 M4 不得直接與 M5 通訊)，全數透過星型拓樸 (Star Topology) 經由核心匯流排交換。



## 3. Validation Traceability (驗證溯源)

* **Item 1: 關注點分離與無狀態邊界 (Stateless Boundary)**
* **Evidence (客觀證據):** `aq_node.py` (M5) 與 `noise_node.py` (M4) 中，強迫指派 `msg.system_state = SafetyState.STATE_NORMAL`。且 M4 中限制 `msg.pm25 = 0.0`，M5 中限制 `msg.noise_db = 0.0`。
* **Architecture Inference (架構推導):** 確立邊緣感測器的無狀態特性。剝離感測器節點的本地閾值判定邏輯，確保全域策略的單一真理來源 (Single Source of Truth) 維持在 Core FSM 中。 `[Ref: ADR-0011]`


* **Item 2: 內建混沌測試與執行期驗證 (Chaos Engineering)**
* **Evidence (客觀證據):** `access_node.py` (M3) 實作 `ChaosTesterNode`，以 10Hz 頻率發布隨機負載 (包含合法請求與 `SQL_INJECTION_OR_GARBAGE`)，並統計 `granted_during_normal`, `blocked_by_safety` 等變數。
* **Architecture Inference (架構推導):** 提供了執行期 (Runtime) 的 SIL (Software-in-the-Loop) 測試框架。客觀量測並驗證了中央 FSM 防禦邏輯的實際生效狀況與資料免疫力。 `[Ref: ADR-0012]`


* **Item 3: UI 崩潰實體隔離 (UI Crash Isolation)**
* **Evidence (客觀證據):** `display_node.py` (M6) 的 Curses 繪圖迴圈中，使用 `try-except curses.error: pass` 包覆所有的終端機繪圖指令。
* **Architecture Inference (架構推導):** 阻斷應用層的例外錯誤 (Exception) 向上蔓延。確保顯示節點的環境脆弱性 (如終端機視窗尺寸縮放) 不會干擾背景資料的持續訂閱與接收。 `[Ref: ADR-0013]`



## 4. Future Evolution & Triggers (未來演進與觸發條件)

* **Evolution Item 1: 實體感測器驅動對接 (Physical Sensor Integration)**
* **Trigger:** 當系統進入 HIL (Hardware-in-the-Loop) 測試階段，需將 M4/M5 替換為真實的硬體感測器 (如 MQ-135, ZE25-O2, 分貝計) 以進行現場校正時。
* **Expected Impact (預期影響):** 將引入硬體相依性 (Hardware Dependency) 與底層通訊延遲，但受惠於無狀態邊界設計，此變更將被侷限於 M4/M5 節點內部，不會影響任何北向的 ROS 2 發布邏輯與核心狀態機。


* **Evolution Item 2: 微服務容器化與資源限制 (Containerization)**
* **Trigger:** 當 M3 (混沌測試) 或其他應用服務的 CPU 佔用率過高，可能影響 Core Bridge 的即時運算能力，且系統進入正式部署 (Production Deployment) 階段時。
* **Expected Impact (預期影響):** 達成更嚴格的 OS 級隔離 (利用 Cgroups 限制 CPU/Memory 資源) 與獨立生命週期管理；但會增加邊緣設備的記憶體開銷與部署流水線 (CI/CD) 的複雜度。


