# ADR-0012: 將混沌測試 (Chaos Testing) 作為內建微服務運行

* **ID:** ADR-0012
* **Date:** 2026-08-01
* **Status:** Accepted
* **Custodian:** `m3_access_control`

---

## 1. Context / Problem (背景與問題)

單靠 CI/CD 階段的靜態測試或傳統的整合測試，無法真實反映分散式系統在執行期（Runtime）的行為。網路的非同步時序、突發的大量併發請求（Concurrency），甚至是惡意負載注入，往往會導致未預期的「狀態爆炸」或防禦穿透。我們需要在執行期持續驗證中央 FSM 的防禦有效性。

## 2. Decision (決策內容)

將混沌測試（Chaos Engineering）直接內建為系統的常駐微服務。在 M3（門禁存取）節點中實作 `ChaosTesterNode`，在系統運行期間持續對核心大腦注入高頻的隨機與惡意負載。

## 3. Rationale (決策理據)

以資料驅動（Data-driven）的方式，在真實的執行時序下，客觀量測並驗證架構約束（如：緊急狀態下必須封鎖所有門禁請求）是否真的生效，確保防禦網在壓力下不會破功。

## 4. Alternatives (替代方案)

* **Alternative A:** 僅在 CI/CD 流水線中撰寫單元測試與模擬整合測試，系統上線後關閉所有測試節點以節省資源。

## 5. Rejected Alternatives (拒絕原因)

* **Rejected Alternative A because:** 離線測試無法暴露 ROS 2 DDS 中介軟體在真實網路環境下可能引發的封包遺失、時序錯亂與競態條件。

## 6. Consequences (預期影響與取捨)

* **Positive:** 獲得了執行期的 SIL (Software-in-the-Loop) 測試框架，隨時產出 `granted_during_normal`, `blocked_by_safety` 等客觀防禦量測指標。
* **Negative:** 佔用邊緣設備的運算資源。以 10Hz 頻率持續發布亂碼與請求，會增加 CPU 佔用率與內部網路負載；未來需依賴 Cgroups 或 Docker 限制其資源，避免拖垮核心業務。

## 7. Related AKB (關聯架構知識庫)

* [`AKB / 06_Service.md`](../AKB/06_Service.md)
* **Concept:** Chaos Engineering (混沌工程)、SIL (Software-in-the-Loop)

## 8. Evidence / References (證據與參考資料)

* 實作證據：[`access_node.py`](../../it_edge_layer/ros2_ws/src/services/m3_access_control/m3_access_control/access_node.py) 內的 `ChaosTesterNode` 以 10Hz 發布包含 `SQL_INJECTION_OR_GARBAGE` 的惡意負載，並監聽 Core Bridge 的防禦裁決。