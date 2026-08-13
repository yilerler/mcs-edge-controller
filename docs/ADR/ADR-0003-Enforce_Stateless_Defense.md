# ADR-0003: 徹底剝離業務邏輯，維持無狀態防禦 (Stateless Defense)

* **ID:** ADR-0003
* **Date:** 2026-08-01
* **Status:** Accepted
* **Custodian:** `ot_defense_layer`

---

## 1. Context / Problem (背景與問題)
硬體感測層直接接觸現場數據（如 RFID 讀卡、距離感測）。若將業務識別（如「這張卡號是否為授權人員」）與實體防禦（如「有人闖入，觸發煞車」）混雜在一起，會導致 Kernel Module 必須處理資料庫比對或網路同步，造成系統狀態嚴重糾纏（Coupling）。

## 2. Decision (決策內容)
將所有存取控制（如門禁判定、RFID 驗證）與業務邏輯從 Kernel Space 徹底卸除，移交給 IT 層的策略引擎處理。OT 層僅負責忠實反映物理環境數值（被動輪詢）與執行最底層的安全阻斷。

## 3. Rationale (決策理據)
確保「機制（Mechanism）」與「策略（Policy）」的絕對分離。將決策大權收斂至 IT 領域策略引擎，確保底層物理防護不被 IT 應用的複雜狀態、網路同步延遲與不可預期風險干擾。

## 4. Alternatives (替代方案)
* **Alternative A:** 直接在 Kernel Module 內部進行 RFID 卡號比對與門禁授權名單判斷，一旦吻合即驅動 GPIO 開門。

## 5. Rejected Alternatives (拒絕原因)
* **Rejected Alternative A because:** 嚴重違反關注點分離（Separation of Concerns）。授權名單是高度變動的業務狀態，若放在 Kernel 中，將導致每次門禁權限異動皆需重新掛載 Kernel Module。這不僅大幅提高系統維護成本，更會為底層核心帶來巨大的資安漏洞與攻擊面。

## 6. Consequences (預期影響與取捨)
* **Positive:** OT 模組極度純淨、穩定，僅專注於絕對的實體安全界線（如 `CRITICAL_DISTANCE_MM`）。升級 IT 業務邏輯時，底層驅動完全不需改動。
* **Negative:** 架構通訊路徑變長。一次門禁刷卡必須歷經「硬體 -> LKM -> Gateway -> Core Bridge」的長鏈路才能完成授權判定，增加了系統內部的通訊中繼（Hop）與微秒級延遲。

## 7. Related AKB (關聯架構知識庫)
* [`AKB / 02_OT_Defense_Layer.md`](../AKB/02_OT_Defense_Layer.md)
* [`AKB / 04_Core_Runtime.md`](../AKB/04_Core_Runtime.md)
* **Concept:** Stateless Boundary (無狀態邊界)、Separation of Concerns (關注點分離)

## 8. Evidence / References (證據與參考資料)
* 實作證據 1：[`mock_elc_core.c`](../../ot_defense_layer/src/mock_elc_core.c) 的 `elc_ioctl` 實作僅執行純粹的記憶體複製（`copy_to_user` / `copy_from_user`），內部無任何身分識別的 `if-else` 分支。
* 實作證據 2：輪詢執行緒具備自主判斷物理越界的防護邏輯（`if (mock_distance < CRITICAL_DISTANCE_MM) { ... = V5_FENCE_BRAKING; }`），證明其防護行為不依賴業務身分。