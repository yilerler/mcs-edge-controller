# Services (應用服務與微服務層)

## 系統定位 (Verified Narrative)
本子系統實作為 IT 邊緣層的周邊感測器、致動器模擬與使用者介面（Edge Microservices）。
作為獨立運行的 ROS 2 Python 節點群，它完美實踐了「無狀態邊界（Stateless Boundary）」與「關注點分離（Separation of Concerns）」原則。透過將硬體 I/O、UI 渲染與高頻壓力測試等非核心邏輯徹底抽離，它確保了中央的 C++ 領域策略引擎（Core FSM）能以最高效能且不被干擾的方式，專注執行全域的安全防禦。

---

## 1. Architecture (架構意圖與邊界)

### 存在目的 (Purpose)
承載所有與「核心決策無關」的周邊與應用邏輯。將環境感測的數位化、本地端戰情面板顯示，以及系統免疫力的壓力驗證剝離至獨立行程（Process）中，確保大腦（Core Bridge）的絕對純淨。

### 互動關係 (Relationships)
本模組採用標準的**星型拓樸（Star Topology）**，所有微服務皆圍繞著 Core Bridge 運作，且微服務彼此之間**零耦合（Zero Coupling）**（例如 M4 絕對不知道 M5 的存在）：
*   **環境感測 (M4, M5)：** 單向向大腦發布（Publish）物理數據。
*   **UI 顯示 (M6)：** 單向訂閱（Subscribe）大腦的語意狀態（唯讀，不干預任何狀態）。
*   **混沌測試 (M3)：** 雙向互動，發布惡意門禁請求並訂閱語意狀態來驗證防禦結果。

### ⛔ 絕對邊界 (Architecture Boundaries)
本層級嚴守最高級別的架構紀律，確立了無懈可擊的無狀態邊界：
*   **零決策權越界 (Zero Decision Power)：** 所有的感測節點絕對不進行業務判斷。例如，即使 PM2.5 飆高到 200，M5 也絕對不宣告 `WARNING`，而是強制將狀態鎖死為 `STATE_NORMAL`，將決策權 100% 委派給中央 FSM。
*   **UI 崩潰絕對隔離 (UI Crash Isolation)：** 顯示節點（M6）的脆弱性絕對不影響資料接收。任何終端機視窗過小導致的 Curses 繪圖越界，都必須在邊界被靜默吞噬（Silently Ignored），阻斷 Crash 向上層蔓延。

---

## 2. Implementation (實作機制與客觀證據)

本模組的架構純度，由以下 Repository 實作證據與動態行為支撐：

*   **強型別合約與無狀態驗證：**
    所有節點均嚴格匯入了 `v5_interfaces.msg` 中的強型別合約（`SafetyState`, `OTState`）。在 M4 (`msg.pm25 = 0.0`) 與 M5 (`msg.noise_db = 0.0`) 的程式碼實作中，客觀證明了領域隔離與關注點分離。
*   **內建混沌測試引擎 (Chaos Engineering - M3)：**
    實作了強大的 SIL (Software-in-the-Loop) 測試框架。`ChaosTesterNode` 以 10Hz 的超高頻率注入隨機惡意負載（包含合法請求、連點攻擊與 `SQL_INJECTION_OR_GARBAGE` 亂碼）。
*   **Runtime 防禦量測指標 (Metrics)：**
    M3 動態觀測並產出 `total_requests`, `granted_during_normal`, `blocked_by_safety`, `invalid_payloads` 等客觀指標，在執行期（Runtime）直接證明了中央大腦的防禦行為生效與絕對穩定性。
*   **UI 例外攔截：**
    `M6DisplayNode` 在繪圖邏輯中實作了明確的 `except curses.error: pass`，提供了物理層級的 Crash 隔離證據。

---

## 3. Engineering Assessment (工程限制與設計取捨)

> **架構取捨決策 (Architecture Decisions & Trade-offs)**

*   **Decision (決策):** 採用「無狀態邊界（Stateless Boundary）」與星型拓樸，感測微服務不進行任何本地閾值過濾。
*   **Constraint (限制):** 即使是毫無變化的正常環境數據（如持續安靜的噪音、正常的 PM2.5），微服務仍會以 10Hz 的高頻率持續向 DDS 匯流排發布 Topic，佔用內部網路頻寬。
*   **Trade-off (取捨):** 我們自願放棄了「邊緣節點的資料過濾與降頻（Data Filtering at the Edge）」，以換取 **「全域策略的單一真理來源（Single Source of Truth）」**。這確保了所有安全閾值（Thresholds）都統一由 Core FSM 集中管理，不會散落在各個 Python 腳本中，這是在工業級架構中為了「極致的維護性」所做出的理性代價。

---

## 4. Future Evolution (未來演進方向)

目前微服務的解耦程度已達到預期目標，未來的演進將聚焦於部署與實體化：
1.  **實體感測器對接 (Physical Sensor Integration)：** 將 M4 (噪音) 與 M5 (空氣品質) 的隨機亂數產生器（Mock），抽換為真實的 I2C/UART 感測器驅動，而無需修改任何北向的 ROS 2 發布邏輯。
2.  **微服務容器化 (Containerization)：** 基於各節點之間零耦合的特性，評估將 M3~M6 節點打包為獨立的 Docker 容器，利用 Kubernetes 或 Docker Compose 進行資源限制（如限制 M3 的 CPU 使用率），實現更嚴格的 OS 級隔離。