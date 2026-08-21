# 04_Core_Runtime.md

## 1. Identity & Purpose (身分與目的)

* **Module Name:** `v5_core_bridge` (核心狀態協調與橋接節點)
* **Architecture Level:** `Level 1 (Edge Semantic Brain / Domain Policy Engine)`
* **Role:** IT 邊緣層的狀態匯流排與決策中樞。
* **Core Responsibility:** 匯總 OT 實體狀態與 IT 環境感測數據，執行有限狀態機（FSM）邏輯推演全域安全語意（Semantic State），並作為唯一合法的決策者向其他微服務發布仲裁結果。

## 2. Architecture Boundary (架構邊界)

本模組強制實施**六角形架構 (Hexagonal Architecture / Ports & Adapters)**，將邊界嚴格劃分為外層的「ROS 2 基礎設施」與內層的「純 C++ 策略大腦」。

* **外殼層 (Infrastructure Shell / ROS 2 Node):**
	* **Inbound Ports:** 訂閱 `OTState.msg`, `CloudIntent.msg`, 以及各類感測 Topic。負責拆解 ROS 2 封包、管理時間戳記，並將資料 Mapping 為純 C++ 基本型別。
	* **Outbound Ports:** 將決策結果打包為 `SafetyState.msg` 並發布至 DDS 匯流排。
	* **邊界防護 (Intent Veto & Watchdog):** 在外殼層實施意圖攔截與超時監控。當處於危險狀態時直接拋棄雲端指令；當偵測到底層心跳逾時（>1.0s），主動竄改輸入值以觸發核心降級。


* **領域層 (Domain Core / Pure C++ FSM):**
	* **Strict Constraints (嚴格限制):**
		* **零框架相依：** 內部絕對禁止出現任何 `<rclcpp/rclcpp.hpp>` 或 DDS 相關依賴。
		* **單向狀態推演：** 僅具備根據輸入計算輸出的純函式 (Pure Function) 特性，絕不主動發起外部網路 I/O 或向下反控 OT 層。



### 📊 Canonical Diagram: Hexagonal FSM & Intent Veto

本圖表呈現 Level 1 核心引擎的同心圓隔離邊界。展示了基礎設施外殼如何保護內部決策核心，以及看門狗與意圖否決的具體攔截點（參見 Section 3）。

```text
               [ CloudIntent ]   [ OTState ]   [ Env / Door Requests ]
                      │               │                  │
 ╔════════════════════│═══════════════│══════════════════│════════════════╗
 ║ ROS 2 Shell        │               │                  │                ║
 ║ (V5CoreBridge_    [▼]             [▼]                [▼]               ║
 ║  Node)         [ VETO ]       (Watchdog)         (Watchdog)            ║
 ║                [BARRIER]        (>1.0s)            (>3.0s)             ║
 ║                    │               │                  │                ║
 ║  ─ ─ ─ ─ ─ ─ ─ ─ ─ ┼ ─ ─ ─ ─ ─ ─ ─ ┼ ─ ─ ─ ─ ─ ─ ─ ─  ┼ ─ ─ ─ ─ ─ ─ ─  ║
 ║ Pure C++ Core      ▼               ▼                  ▼                ║
 ║ (V5SafetyFSM)  ( double )      ( uint8_t )       ( bool / double )     ║
 ║               [ threshold]    [ ot_level ]      [ m4_offline... ]      ║
 ║                    │               └────────┬─────────┘                ║
 ║                    ▼                        ▼                          ║
 ║              ((  V5SafetyFSM::evaluate( primitives )  )) ◄══[ TICK ]   ║
 ║                                             │                 TIMER    ║
 ║                                             ▼                  (50ms)  ║
 ║                                   [ out_sys_state ]                    ║
 ║  ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─  │ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─  ║
 ║                                             │                          ║
 ║                                       (Message Pack)                   ║
 ║                                             ▼                          ║
 ╚═════════════════════════════════════════════│══════════════════════════╝
                                               ▼
                                      [ SafetyState.msg ]

```

> **Visual Semantics (視覺語意與證據綁定)**
> * `╔═ / ─ ─` **(Hexagonal Isolation):** 雙層隔離邊界。對應 `Item 1`，由實作層中 `V5SafetyFSM::evaluate()` 僅接收純 C++ 基本型別 (`uint8_t`, `double`, `bool`) 提供證據。
> * `[ VETO BARRIER ]`: 意圖否決屏障。對應 `Item 2`，由外殼層 `intent_callback` 中 `if (sys_state_ == EMERGENCY) return;` 的直接阻斷邏輯提供證據。
> * `[ TICK TIMER (50ms) ]`: 時間驅動脈搏。對應 `Item 3`，由外殼層 `timer_` 的 50ms (20Hz) 定期觸發提供證據。
> * `(Watchdog)`: 超時竄改降級。對應 `Item 3`，由外殼層 `fsm_tick` 中 `(now - last_time) > 1.0` 強制設定 `latest_ot_sys_level_ = 2` 的邏輯提供證據。
> 
> 

---

## 3. Validation Traceability (驗證溯源)

* **Item 1: 領域與外殼解耦 (Hexagonal Architecture Isolation)**
	* **Evidence (客觀證據):**
		* [`bridge_node.cpp`](../../it_edge_layer/ros2_ws/src/core/v5_core_bridge/src/bridge_node.cpp) 中，`V5SafetyFSM::evaluate()` 宣告為純 C++ 靜態函式，內部無任何 `rclcpp` 依賴。
		* ROS 2 的 Topic 收發、型別轉換 (Type Mapping) 與時間戳記管理，全數集中且受限於 `V5CoreBridgeNode` 類別 (Infrastructure Shell) 中。


	* **Architecture Inference (架構推導):** 將非同步的通訊基礎設施與核心業務邏輯徹底物理隔離，確保決策引擎的絕對純淨性。這讓大腦免於 Race Condition 威脅，並能在無硬體與無 ROS 2 框架的環境下執行 100% 覆蓋率的邏輯單元測試。 Ref: [`ADR-0005`](../ADR/ADR-0005-Decouple_FSM_With_Hexagonal_Architecture.md)


* **Item 2: 邊緣意圖否決權 (Intent Veto & Edge Sovereignty)**
	* **Evidence (客觀證據):** 外殼層的 `intent_callback` 實作中，明確包含了 `if (sys_state_ == STATE_EMERGENCY) { return; }` 強制中斷指令，以及針對 `desired_pm25_threshold` 的數值上下限安全檢查。
	* **Architecture Inference (架構推導):** 確立了邊緣端的最高決策優先權與**意圖邊界過濾 (Intent Filtering)**。當實體發生災難時，系統在外殼層 (Shell) 直接阻斷雲端指令，防止惡意或錯誤的配置意圖滲透進純淨的 FSM 覆蓋本地防禦。 Ref: [`ADR-0006`](../ADR/ADR-0006-Edge_Intent_Veto_Right.md)


* **Item 3: 全域看門狗與主動降級 (Global Watchdog Degradation)**
	* **Evidence (客觀證據):** 外殼層的 `fsm_tick` 定時器中，實作了針對 `last_ot_heartbeat_time_`、`last_m4_time_` 等變數的超時判定 (`> 1.0s` 與 `> 3.0s`)，並在超時時直接竄改輸入變數。
	* **Architecture Inference (架構推導):** 確立了系統時間驅動 (Time-driven) 的 Fail-Safe 機制。當南向實體設備或同層級微服務發生靜默崩潰 (Silent Failure) 導致資料逾時，定時器 (Tick) 將強制觸發狀態更新，系統不再信賴過期快取，主動切換至 `STATE_DEGRADED` 以限縮風險。 Ref: [`ADR-0007`](../ADR/ADR-0007-Timestamp_Based_Global_Watchdog.md)



## 4. Future Evolution & Triggers (未來演進與觸發條件)

* **Evolution Item 1: DDS QoS 策略調校 (Quality of Service Tuning)**
	* **Trigger:** 當 IT 網域內微服務數量擴張，導致內部網路壅塞，且觀測到 `semantic_state` 發生 Packet Loss（封包遺失），進而影響門禁放行時效與安全性時。
	* **Expected Impact (預期影響):** 提升關鍵決策傳遞的可靠度（設定為 Reliability: Reliable），但將增加 DDS Middleware 的記憶體消耗（需保留 History Buffer），並可能略微增加系統整體的傳輸延遲。


* **Evolution Item 2: 導入 Zero-Copy 傳輸機制**
	* **Trigger:** 當系統整合高頻/大頻寬感測器（如影像串流、光達），且訊息的序列化/反序列化（Serialization）造成 Bridge Node 的 CPU 負載常態性超越 80% 時。
	* **Expected Impact (預期影響):** 可大幅降低 CPU 負載與傳輸延遲，但將限制發布者與訂閱者必須運行於同一實體主機（Localhost IPC），破壞分散式部署的彈性，並增加共享記憶體管理的複雜度。


