# 06_Service.md

## 1. Identity & Purpose (身分與目的)

* **Module Name:** `services` (`m3_access_control`, `m4_environment_monitor`, `m5_air_quality_monitor`, `m6_local_display`)
* **Architecture Level:** `Level 3 (Stateless Edge Microservices & Applications)`
* **Role:** IT 邊緣層的周邊感測器、致動器模擬與使用者介面。
* **Core Responsibility:** 承載與核心決策無關的周邊與應用邏輯，包含環境感測訊號數位化、本地端戰情面板顯示，以及執行期的系統壓力與邊界防禦驗證。

## 2. Architecture Boundary (架構邊界)

本層級嚴格實施**微服務孤島化 (Microservice Islanding)**。Level 3 不是一個實體的「層」，而是多個彼此互不相見的執行期實體集合：

* **拓樸約束 (Topology Constraint):**
	* **零水平耦合 (Zero Horizontal Coupling):** 節點之間（如 M4 與 M5）絕對禁止任何形式的直接 P2P 訂閱或發布。全數通訊強制依循**星型拓樸 (Star Topology)** 經由 DDS 匯流排與 Level 1 (Core Bridge) 進行垂直交換。


* **權力約束 (Authority Constraint):**
	* **零決策權越界 (Zero Decision Power):** 感測節點 (M4/M5) 雖為了介面統一而復用 `SafetyState.msg` 作為遙測載體，但其內部的 `system_state` 欄位被強制鎖死為靜態常數 (如 `STATE_NORMAL`)，徹底剝奪本地閾值判定與警報決策權，100% 委派給中央大腦。
	* **應用崩潰隔離 (Crash Isolation):** UI 介面 (M6) 或外部模擬器 (M3) 的任何例外錯誤 (Exceptions) 必須被封裝在節點內部，絕對不允許影響 ROS 2 網域的健康度或干擾核心運作。



### 📊 Canonical Diagram: Star Topology & Stateless Islands

本圖表呈現 Level 3 微服務的孤島架構。強調了平行節點之間的絕對隔離（禁止橫向連線），以及強制剝奪決策權的硬編碼機制（參見 Section 3）。

```text
               (To/From Level 1 Core Bridge via ROS 2 DDS Bus)
        ▲                 ▲                 ▲                    │
        │(Telemetry Pack) │(Telemetry Pack) │(Door Request)      │(SafetyState)
        │                 │                 │(Chaos Pack)        │(OTState)
        │                 │                 │                    ▼
+-------│-----------------│-----------------│--------------------│----------+
|       │                 │                 │                    │          |
|  +====│=============+   │                 │          +=========│=======+  |
|  | [ M4: Noise ]    |   │                 │          | [ M6: Display ] |  |
|  |                  |   │                 │          |                 |  |
|  | [ STATE STRIPPED]|   │                 │          | [ CRASH SHIELD ]|  |
|  | (sys_state=0)    |   │                 │          | (try-except)    |  |
|  +==================+   │                 │          +=================+  |
|                         │                 │                               |
|               +=========│=======+         │                               |
|               | [ M5: PM2.5 ]   |         │                               |
|               |                 |         │                               |
|               | [ STATE STRIPPED]         │                               |
|               | (sys_state=0)   |         │                               |
|               +=================+         │                               |
|                                           │                               |
|                                 +=========│=======+                       |
|                                 | [ M3: Access ]  |                       |
|                                 |                 |                       |
|                                 | [ CHAOS ENGINE ]|                       |
|                                 | (10Hz Garbage)  |                       |
|                                 +=================+                       |
|                                                                           |
|          [ ZERO HORIZONTAL COUPLING (NO LATERAL CONNECTIONS) ]            |
+---------------------------------------------------------------------------+

```

> **Visual Semantics (視覺語意與證據綁定)**
> * `[ ZERO HORIZONTAL COUPLING ]`: 象徵絕對的星型拓樸。圖面上所有箭頭僅能垂直指向外部 DDS 匯流排，M3~M6 之間無任何連線。
> * `[ STATE STRIPPED ]`: 決策權剝奪。對應 `Item 1`，由 `aq_node.py` 與 `noise_node.py` 中強迫指派 `msg.system_state = STATE_NORMAL` 的硬編碼證據支持。
> * `[ CHAOS ENGINE ]`: 壓力產生器。對應 `Item 2`，由 `access_node.py` 內部以 10Hz 觸發混亂字串的邏輯支持。
> * `[ CRASH SHIELD ]`: 崩潰隔離區。對應 `Item 3`，由 `display_node.py` 迴圈外部的 `try-except curses.error` 捕捉機制支持。
> 
> 

---

## 3. Validation Traceability (驗證溯源)

* **Item 1: 關注點分離與無狀態邊界 (Stateless Boundary & Power Stripping)**
	* **Evidence (客觀證據):** [`aq_node.py (M5)`](../../it_edge_layer/ros2_ws/src/services/m5_air_quality_monitor/m5_air_quality_monitor/aq_node.py) 與 [`noise_node.py (M4)`](../../it_edge_layer/ros2_ws/src/services/m4_environment_monitor/m4_environment_monitor/noise_node.py) 中，不論感測數值高低，皆強迫指派 `msg.system_state = SafetyState.STATE_NORMAL`。且實施嚴格欄位隔離：M4 中限制 `msg.pm25 = 0.0`，M5 中限制 `msg.noise_db = 0.0`。
	* **Architecture Inference (架構推導):** 確立邊緣感測器的絕對無狀態特性。透過強制欄位覆寫，剝離感測器節點的本地閾值判定邏輯與越界干擾能力，確保全域策略的單一真理來源 (Single Source of Truth) 100% 維持在 Core FSM 中。 Ref: [`ADR-0011`](../ADR/ADR-0011-Stateless_Boundary_And_Star_Topology.md)


* **Item 2: 內建混沌測試與執行期驗證 (Chaos Engineering)**
	* **Evidence (客觀證據):** [`access_node.py (M3)`](../../it_edge_layer/ros2_ws/src/services/m3_access_control/m3_access_control/access_node.py) 除了發送常規門禁字串外，特別實作了 `ChaosTesterNode`，以 10Hz 高頻發布隨機負載 (包含合法請求與 `SQL_INJECTION_OR_GARBAGE` 等預期外的毒性資料)，並統計 `granted_during_normal`, `blocked_by_safety` 等變數。
	* **Architecture Inference (架構推導):** 提供了執行期 (Runtime) 的 SIL (Software-in-the-Loop) 測試與驗證框架。這不僅模擬了負載，更客觀量測並驗證了中央 FSM (Level 1) 與 Gateway (Level 2) 在面對惡意高頻注入時的防禦邏輯生效狀況與資料免疫力。 Ref: [`ADR-0012`](../ADR/ADR-0012-Built_In_Chaos_Testing.md)


* **Item 3: UI 崩潰實體隔離 (UI Crash Isolation)**
	* **Evidence (客觀證據):** [`display_node.py (M6)`](../../it_edge_layer/ros2_ws/src/services/m6_local_display/m6_local_display/display_node.py) 的 Curses 終端機繪圖迴圈中，使用 `try-except curses.error: pass` 包覆了所有的渲染指令。
	* **Architecture Inference (架構推導):** 阻斷應用層的例外錯誤 (Exception) 向上蔓延。確保顯示節點的環境脆弱性 (如終端機視窗尺寸被不當縮放、字元編碼異常) 被徹底封鎖在 `CRASH SHIELD` 內，不會導致節點當機，更不會干擾背景 DDS 資料的持續訂閱與接收。 Ref: [`ADR-0013`](../ADR/ADR-0013-Isolate_UI_Exceptions.md)



## 4. Future Evolution & Triggers (未來演進與觸發條件)

* **Evolution Item 1: 實體感測器驅動對接 (Physical Sensor Integration)**
	* **Trigger:** 當系統進入 HIL (Hardware-in-the-Loop) 測試階段，需將 M4/M5 替換為真實的硬體感測器 (如 MQ-135, ZE25-O2, 分貝計) 以進行現場校正時。
	* **Expected Impact (預期影響):** 將引入硬體相依性 (Hardware Dependency) 與底層通訊延遲，但受惠於無狀態邊界設計，此變更將被侷限於 M4/M5 節點內部，不會影響任何北向的 ROS 2 發布邏輯與核心狀態機的運算。


* **Evolution Item 2: 微服務容器化與資源限制 (Containerization)**
	* **Trigger:** 當 M3 (混沌測試) 或其他應用服務的 CPU 佔用率過高，可能影響 Core Bridge 的即時運算能力，且系統進入正式部署 (Production Deployment) 階段時。
	* **Expected Impact (預期影響):** 達成更嚴格的 OS 級隔離 (利用 Cgroups 限制 CPU/Memory 資源) 與獨立生命週期管理；但會增加邊緣設備的記憶體開銷與部署流水線 (CI/CD) 的複雜度。


