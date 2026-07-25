# Core Runtime (核心狀態協調與橋接節點)

## 系統定位 (Verified Narrative)
本模組實作為 IT 邊緣層（IT Edge Layer）的狀態匯流排與決策中樞。
作為 IT 與 OT 之間的「大腦與翻譯官」，它採用了**六角形架構（Hexagonal Architecture）**，將 ROS 2 的發布/訂閱（Pub/Sub）通訊外殼，與核心的領域策略引擎（Domain Policy Engine）徹底分離。此節點不僅負責狀態融合，更具備強大的**邊緣主權（Edge Sovereignty）**與防禦降級能力，確保邊緣端在面對雲端或 IT 服務異常時，始終保有最高級別的物理安全主控權。

---

## 1. Architecture (架構意圖與邊界)

### 存在目的 (Purpose)
將底層（OT 防禦層）上報的純物理冰冷數據，與 IT 側的環境感測（如噪音、PM2.5）進行匯總，推演出全域別的**安全語意（Semantic State）**，並作為唯一合法的決策者，將決策結果廣播給 IT 層的其他微服務（如門禁系統與雲端 Gateway）。

### 互動關係 (Relationships)
本模組展現了極致的內外層解耦：
*   **領域核心 (Domain Logic)：** `V5SafetyFSM` 完全不依賴 ROS 2 框架。作為純粹的 C++ 靜態方法，專注於處理變數輸入與輸出。
*   **基礎設施外殼 (Infrastructure Shell)：** `V5CoreBridgeNode` 負責處理 ROS 2 的 Topic 收發與時間戳記管理，將外部事件轉譯為輸入參數餵給核心 FSM。

### ⛔ 絕對邊界 (Architecture Boundaries)
本節點嚴守「OT 為物理唯一真理」的原則，並確立以下不可逾越的架構禁區：
*   **南向邊界（對 OT 絕對服從與零控制）：** 接收資料嚴格對應 12 Bytes 的 ABI 合約（`OTState.msg`）。`ot_callback` 僅被動接收狀態，**絕對不反向控制 OT 底層的行為**。
*   **北向邊界（不處理業務驗證）：** 本節點**絕對不處理**具體的「身份驗證」（如比對 RFID 卡號或密碼）。它僅接收微服務的 `OPEN` 請求，並基於「當下全域安全狀態」給出最終放行或鎖定的裁決（Arbitration）。

---

## 2. Implementation (實作機制與客觀證據)

本模組的架構宣稱，均具備以下程式碼層級的動態與靜態證據支撐：

*   **意圖否決與邊緣主權 (Intent Veto & Edge Sovereignty)：**
    透過 `intent_callback` 實作了防禦阻斷邏輯。當系統處於緊急狀態（如 `sys_state_ == STATE_EMERGENCY`）或判定雲端下達的閾值不合理時，Bridge Node 會直接拒絕套用來自雲端的 `CloudIntent`（配置意圖），確保安全不被遠端錯誤指令覆蓋。
*   **全域狀態看門狗 (Global Watchdog Degradation)：**
    透過 `timer_`（50ms 週期）與 `fsm_tick` 內建超時偵測。系統持續監控 OT（`last_ot_heartbeat_time_`）及 IT 感測器的時間戳記，一旦發現心跳逾時（如 `> 1.0` 秒），將主動觸發系統安全降級（`STATE_DEGRADED`）。
*   **語意狀態轉譯 (Semantic Translation)：**
    發布的 Topic 不再是原始感測值，而是經過 FSM 詮釋後的 `semantic_state`，並帶有 `override_source` 與 `intent_version_hash`，為系統提供「配置對帳」與「決策溯源」的能力。

---

## 3. Engineering Assessment (工程限制與設計取捨)

> **架構取捨決策 (Architecture Decisions & Trade-offs)**

*   **Decision (決策):** 採用六角形架構將 FSM 從 ROS 2 Node 中完全抽離，並以 ROS 2 Pub/Sub 作為內部微服務的通訊匯流排。
*   **Constraint (限制):** 所有的物理狀態在進入 IT 側後，皆須經過 ROS 2 的序列化/反序列化（Serialization）與 Middleware (DDS) 層的傳遞。
*   **Trade-off (取捨):** 為了換取 **「極高的單元測試可行性（Testability）」**與 **「跨語言微服務擴展性」**，我們接受了 ROS 2 DDS 在節點間通訊所帶來的微秒級傳輸開銷（Overhead）。這是為了支撐 IT 層龐大且多語言（Polyglot）業務邏輯的理性決策。

---

## 4. Future Evolution (未來演進方向)

基於目前作為「狀態匯流排」的定位與 ROS 2 通訊特性，未來的架構演進將評估：
1.  **DDS QoS 策略調校 (Quality of Service Tuning)：** 針對 `semantic_state` 等關鍵決策 Topic，設定更嚴格的 QoS（如 Reliability: Reliable, History: Keep Last），以防範 IT 層網路壅塞時的 Packet Loss。
2.  **Zero-Copy 傳輸評估：** 若未來加入高頻率影像感測數據，將評估在 Bridge Node 與 Gateway 之間導入 ROS 2 借用共享記憶體的 Zero-Copy 機制，以降低系統 CPU 負載。