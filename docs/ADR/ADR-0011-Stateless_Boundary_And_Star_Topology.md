# ADR-0011: 採用無狀態邊界 (Stateless Boundary) 與星型拓樸

* **ID:** ADR-0011
* **Date:** 2026-08-01
* **Status:** Accepted
* **Custodian:** `services` (M3-M6 應用微服務)

---

## 1. Context / Problem (背景與問題)

在物聯網邊緣架構中，開發者常習慣將「警報判斷邏輯」直接寫在感測器節點內（例如：在 Python 腳本中寫入 `if PM2.5 > 100: trigger_warning()`）。若不加以限制，系統的安全閾值與決策規則將四散於各個微服務中。這不僅導致配置對帳（Configuration Reconciliation）變得極度困難，更會引發多個感測器同時觸發不同警報時的「狀態競合（Race Condition）」與邏輯衝突。

## 2. Decision (決策內容)

對所有 Level 3 的感測節點實施嚴格的「無狀態邊界（Stateless Boundary）」與星型拓樸（Star Topology）。感測微服務彼此互不知曉，且被強制剝離所有業務決策權，僅能向中央匯流排發布客觀物理數值，將狀態推演權 100% 委派給 Core FSM。

## 3. Rationale (決策理據)

確保全域策略擁有單一真理來源（Single Source of Truth）。統一由中央大腦管理所有的安全閾值，消滅邏輯碎塊，大幅提升系統的維護性與防禦策略的絕對一致性。

## 4. Alternatives (替代方案)

* **Alternative A:** 在邊緣節點實作資料過濾與降頻 (Data Filtering at the Edge)，讓感測節點自行判定，僅在數值超標時才向外發布警報。

## 5. Rejected Alternatives (拒絕原因)

* **Rejected Alternative A because:** 這會破壞系統決策的集中性。若將判定權下放，中央大腦將失去對全域環境的掌控力，且邊緣節點將被迫引入複雜的狀態儲存機制，增加潛在的非同步 Bug。

## 6. Consequences (預期影響與取捨)

* **Positive:** 感測節點的程式碼變得極度純淨、愚蠢且易於維護，Core Bridge 獲得了系統狀態的絕對壟斷權。
* **Negative:** 佔用內部網路頻寬。即使是毫無變化的正常環境數據（如持續安靜的噪音、正常的 PM2.5），微服務仍必須以 10Hz 的高頻率持續向 DDS 匯流排發布 Topic。

## 7. Related AKB (關聯架構知識庫)

* [`AKB / 06_Service.md`](../AKB/06_Service.md)
* [`AKB / 04_Core_Runtime.md`](../AKB/04_Core_Runtime.md)
* **Concept:** Stateless Boundary (無狀態邊界)、Single Source of Truth (單一真理來源)

## 8. Evidence / References (證據與參考資料)

* 實作證據：[`aq_node.py`](../../it_edge_layer/ros2_ws/src/services/m5_air_quality_monitor/m5_air_quality_monitor/aq_node.py) (M5) 與 [`noise_node.py`](../../it_edge_layer/ros2_ws/src/services/m4_environment_monitor/m4_environment_monitor/noise_node.py) (M4) 中，強迫指派 `msg.system_state = SafetyState.STATE_NORMAL`，證明感測器無權更改狀態。
* 實作證據：M4 中限制 `msg.pm25 = 0.0`，M5 中限制 `msg.noise_db = 0.0`，證明微服務間無水平耦合。