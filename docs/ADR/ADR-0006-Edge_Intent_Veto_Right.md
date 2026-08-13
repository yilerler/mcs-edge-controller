# ADR-0006: 賦予邊緣決策引擎對雲端指令的最終否決權 (Intent Veto)

* **ID:** ADR-0006
* **Date:** 2026-08-01
* **Status:** Accepted
* **Custodian:** `v5_core_bridge`

---

## 1. Context / Problem (背景與問題)
在雲端管理的物聯網架構中，邊緣設備常被視為單純的「指令執行器」。然而，當工地現場發生實體災難（如火災、人員闖入危險區），或是雲端系統遭到入侵而下達錯誤的配置指令（如將 PM2.5 警報閾值調至危險數值）時，若邊緣端盲目服從，將引發嚴重的工安危機。

## 2. Decision (決策內容)
邊緣端的 Core Bridge 保有對雲端配置意圖（`CloudIntent`）的最高決策優先權與最終否決權（Intent Veto）。當實體狀態處於危急或指令越界時，邊緣端可主動拒絕執行雲端指令。

## 3. Rationale (決策理據)
貫徹「邊緣主權（Edge Sovereignty）」的實體安全自治原則。確保系統在離線（Offline）、網路延遲或雲端遭劫持時，仍能維持最高層級的物理防護運作，防堵來自 IT 網路的異常狀態覆蓋本地防禦。

## 4. Alternatives (替代方案)
* **Alternative A:** 將系統的最終決策權（Master Control）交由雲端戰情室統籌，邊緣端僅負責上報資料並被動執行雲端下發的控制指令。

## 5. Rejected Alternatives (拒絕原因)
* **Rejected Alternative A because:** 嚴重違反物理安全設計。一旦遭遇斷網或廣域網路壅塞，工地現場將完全喪失應變與門禁管制能力。物理安全的反應時間要求（毫秒級）遠低於雲端網路的延遲極限。

## 6. Consequences (預期影響與取捨)
* **Positive:** 賦予系統極強的容錯與防禦能力，實現真正的 Edge Autonomy（邊緣自治），確保物理底線不被網路破壞。
* **Negative:** 可能產生「狀態不一致（Configuration Drift）」。雲端面板可能顯示指令已發送，但邊緣端因安全因素將其否決。未來需引入更複雜的「配置對帳與回報機制」，向雲端解釋指令為何被拒絕。

## 7. Related AKB (關聯架構知識庫)
* [`AKB / 04_Core_Runtime.md`](../AKB/04_Core_Runtime.md)
* **Concept:** Edge Sovereignty (邊緣主權)、Intent Veto (意圖否決)

## 8. Evidence / References (證據與參考資料)
* 實作證據：`intent_callback` 中實作了強制的阻斷邏輯 `if (sys_state_ == STATE_EMERGENCY) { return; }`。
* 實作證據：針對 `desired_pm25_threshold` 等雲端意圖，內建了硬編碼（Hard-coded）的數值上下限安全檢查。