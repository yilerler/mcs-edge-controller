# 04_Core_Runtime.md

## 1. Identity & Purpose (身分與目的)

* **Module Name:** `v5_core_bridge` (核心狀態協調與橋接節點)
* **Architecture Level:** `Level 1 (Edge Semantic Brain / Domain Policy Engine)`
* **Role:** IT 邊緣層的狀態匯流排與決策中樞。
* **Core Responsibility:** 匯總 OT 實體狀態與 IT 環境感測數據，執行有限狀態機（FSM）邏輯推演全域安全語意（Semantic State），並作為唯一合法的決策者向其他微服務發布仲裁結果。

## 2. Architecture Boundary (架構邊界)

* **Inbound (接收):** [`OTState.msg`](../../it_edge_layer/ros2_ws/src/core/v5_interfaces/msg/OTState.msg)（底層物理真理）、[`CloudIntent.msg`](../../it_edge_layer/ros2_ws/src/core/v5_interfaces/msg/CloudIntent.msg)（雲端配置意圖）、環境感測 Topic（噪音、PM2.5），以及門禁微服務的 `OPEN/CLOSE` 請求。
* **Outbound (輸出):** 包含全域系統狀態與門禁裁決結果的 [`SafetyState.msg`](../../it_edge_layer/ros2_ws/src/core/v5_interfaces/msg/SafetyState.msg)。
* **Strict Constraints (嚴格限制):**
* **不反向控制 OT 底層：** 針對 OT 層僅具備讀取權限，絕不向下發送控制指令，維持單向資料流。
* **不處理業務身份驗證：** 節點內禁用 RFID 卡號比對或密碼驗證邏輯；僅根據「當下全域安全狀態」對已驗證的存取請求進行最終的布林值（Boolean）裁決（放行/鎖定）。



## 3. Validation Traceability (驗證溯源)

* **Item 1: 領域與外殼解耦 (Hexagonal Architecture)**
* **Evidence (客觀證據):** `V5SafetyFSM::evaluate()` 宣告為純 C++ 靜態函式，內部無任何 `rclcpp` 依賴；ROS 2 的 Topic 收發與時間戳記管理全數集中於 `V5CoreBridgeNode` 類別中。
* **Architecture Inference (架構推導):** 將通訊基礎設施與核心業務邏輯徹底分離，確保決策引擎的純淨性，使其能在無硬體與無 ROS 2 框架的環境下執行 100% 的邏輯單元測試。 Ref: [`ADR-0005`](../ADR/ADR-0005-Decouple_FSM_With_Hexagonal_Architecture.md)


* **Item 2: 邊緣意圖否決權 (Intent Veto & Edge Sovereignty)**
* **Evidence (客觀證據):** `intent_callback` 實作中包含 `if (sys_state_ == ... STATE_EMERGENCY) { return; }` 與針對 `desired_pm25_threshold` 的數值上下限檢查。
* **Architecture Inference (架構推導):** 確立了邊緣端的最高決策優先權。當實體發生災難或雲端配置越界時，系統強制阻斷配置更新，防止雲端錯誤指令覆蓋本地的安全狀態。 Ref: [`ADR-0006`](../ADR/ADR-0006-Edge_Intent_Veto_Right.md)


* **Item 3: 全域看門狗與主動降級 (Global Watchdog Degradation)**
* **Evidence (客觀證據):** `fsm_tick` 定時器中，實作了針對 `last_ot_heartbeat_time_`、`last_m4_time_` 等變數的 `(now - last_time).seconds() > threshold` 超時判定。
* **Architecture Inference (架構推導):** 確立了系統的 Fail-Safe 機制。當南向實體設備或同層級微服務發生斷線或崩潰時，系統不再信賴過期快取，並主動切換至 `STATE_DEGRADED` 狀態以限縮潛在風險。 Ref: [`ADR-0007`](../ADR/ADR-0007-Timestamp_Based_Global_Watchdog.md)



## 4. Future Evolution & Triggers (未來演進與觸發條件)

* **Evolution Item 1: DDS QoS 策略調校 (Quality of Service Tuning)**
* **Trigger:** 當 IT 網域內微服務數量擴張，導致內部網路壅塞，且觀測到 `semantic_state` 發生 Packet Loss（封包遺失），進而影響門禁放行時效與安全性時。
* **Expected Impact (預期影響):** 提升關鍵決策傳遞的可靠度（設定為 Reliability: Reliable），但將增加 DDS Middleware 的記憶體消耗（需保留 History Buffer），並可能略微增加系統整體的傳輸延遲。


* **Evolution Item 2: 導入 Zero-Copy 傳輸機制**
* **Trigger:** 當系統整合高頻/大頻寬感測器（如影像串流、光達），且訊息的序列化/反序列化（Serialization）造成 Bridge Node 的 CPU 負載常態性超越 80% 時。
* **Expected Impact (預期影響):** 可大幅降低 CPU 負載與傳輸延遲，但將限制發布者與訂閱者必須運行於同一實體主機（Localhost IPC），破壞分散式部署的彈性，並增加共享記憶體管理的複雜度。


