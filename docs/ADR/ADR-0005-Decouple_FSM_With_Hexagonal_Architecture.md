# ADR-0005: 採用六角形架構將 FSM 從 ROS 2 通訊節點中抽離

* **ID:** ADR-0005
* **Date:** 2026-08-01
* **Status:** Accepted
* **Custodian:** `v5_core_bridge`

---

## 1. Context / Problem (背景與問題)
在開發邊緣控制節點時，若將狀態機（FSM）的業務邏輯直接寫在 ROS 2 的 Topic Callback（回呼函式）中，會導致核心決策邏輯與特定的 Middleware 框架（ROS 2 DDS）產生深度耦合。由於 Callback 屬非同步執行，狀態變數散落其中極易引發 Race Condition（競態條件），且工程師無法在脫離 ROS 環境與實體硬體的情況下，對決策大腦進行純粹的單元測試。

## 2. Decision (決策內容)
採用六角形架構（Hexagonal Architecture），將核心的領域策略引擎（Domain Policy Engine）與 ROS 2 的通訊基礎設施（Infrastructure Shell）徹底物理與邏輯剝離。

## 3. Rationale (決策理據)
確保核心狀態機邏輯的絕對可測試性（Testability）與純淨性。將「I/O 處理（收發 Topic、時間戳記管理）」與「業務推演（計算全域狀態）」分離，確保系統的大腦能在任何框架下獨立運作與驗證。

## 4. Alternatives (替代方案)
* **Alternative A:** 將 `if-else` 的狀態判斷邏輯直接寫入各個感測器 Topic 的 Callback function 內部，藉由全域變數共用狀態。

## 5. Rejected Alternatives (拒絕原因)
* **Rejected Alternative A because:** 狀態變數將散落於各個非同步執行的 Callback 中，極易引發 Race Condition 與狀態不一致。更致命的是，這會讓邏輯測試變得極度困難，必須啟動完整的 ROS 2 網域才能驗證 FSM 的防禦行為。

## 6. Consequences (預期影響與取捨)
* **Positive:** 獲得了 100% 的邏輯單元測試覆蓋率，且隔離了非同步通訊帶來的狀態混亂風險。
* **Negative:** 增加了系統的 Boilerplate Code（樣板程式碼）。所有進入節點的 ROS 2 Message，都必須經過一次額外的轉譯（Mapping），轉換為 C++ 原生結構體後，才能餵給核心 FSM。

## 7. Related AKB (關聯架構知識庫)
* [`AKB / 04_Core_Runtime.md`](../AKB/04_Core_Runtime.md)
* **Concept:** Hexagonal Architecture (六角形架構)、Separation of Concerns (關注點分離)

## 8. Evidence / References (證據與參考資料)
* 實作證據：`V5SafetyFSM::evaluate()` 被宣告為純 C++ 靜態函式，內部**無任何** `rclcpp` 依賴。
* 實作證據：ROS 2 的 Topic 收發與時間戳記管理全數集中且受限於 `V5CoreBridgeNode` 類別中。