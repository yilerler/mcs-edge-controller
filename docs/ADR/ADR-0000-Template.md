# ADR-000: [決策簡短標題 (Title)]

* **ID:** ADR-XXX
* **Date:** YYYY-MM-DD
* **Status:** Proposed / Accepted / Rejected / Superseded
* **Custodian:** [負責捍衛此決策的領域/模組，例如 v5_ioctl_contract]

---

## 1. Context / Problem (背景與問題)
**[Schema 說明]** 描述在做出此決策時，系統面臨的具體痛點、時空背景或技術瓶頸。
*我們遇到了什麼問題？為什麼現有的架構無法解決？*

## 2. Decision (決策內容)
**[Schema 說明]** 具體說明我們決定採取什麼架構行動。這必須是一句或一段明確的宣示。
*(例：Edge retains final veto authority over CloudIntent.)*

## 3. Rationale (決策理據)
**[Schema 說明]** 為什麼這個決策是最佳解？它符合系統的哪些核心原則（如：單一真理來源、物理隔離）？

## 4. Alternatives (替代方案)
**[Schema 說明]** 當時考慮過的其他做法。
* **Alternative A:** ...
* **Alternative B:** ...

## 5. Rejected Alternatives (拒絕原因)
**[Schema 說明]** 基於什麼物理限制或架構原則，我們排除了上述替代方案。
* **Rejected Alternative A because:** (例：會破壞實體邊界的決定性。)
* **Rejected Alternative B because:** (例：會引發嚴重的 Race Condition。)

## 6. Consequences (預期影響與取捨)
**[Schema 說明]** 實施此決策後，系統必須承擔的工程代價（Trade-off）。**這與「拒絕替代方案的原因」不同，這是我們「選擇該決策後必須吞下的苦果或獲得的優勢」。**
* **Positive:** (例：獲得了 O(1) 的解析複雜度與絕對的記憶體安全。)
* **Negative:** (例：喪失了 Payload 的動態擴展性，新增感測器需重新編譯 Kernel Module。)

## 7. Related AKB (關聯架構知識庫)
**[Schema 說明]** 建立 ADR (Why) 與 AKB (What) 之間的雙向連結。列出受此決策影響的 AKB 檔案與核心概念，防止「分流後失聯」。
* `AKB / 03_ICD_Contract.md`
* `AKB / 01_Architecture_Glossary.md`
* **Concept:** Stateless Boundary (無狀態邊界)

## 8. Evidence / References (證據與參考資料)
**[Schema 說明]** 附上能支撐此決策的 PR 連結、GitHub Issue、或是具體的程式碼路徑（如 `#pragma pack(1)` 的實作檔案）。