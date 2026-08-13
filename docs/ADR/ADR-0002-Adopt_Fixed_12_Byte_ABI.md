# ADR-0002: 採用固定的 12-byte ABI 靜態合約

* **ID:** ADR-0002
* **Date:** 2026-08-01
* **Status:** Accepted
* **Custodian:** `v5_ioctl_contract`

---

## 1. Context / Problem (背景與問題)
跨越作業系統層級（Kernel Space 與 User Space）的狀態交換，是系統最脆弱的實體邊界。若採用動態長度或複雜的指標結構進行通訊，極易引發指標越界（Out-of-bounds）、記憶體外洩（Memory Leak）或跨平台編譯器對齊差異（Padding issues），進而導致 Kernel Panic 或被惡意負載攻擊。

## 2. Decision (決策內容)
與 IT 網域的狀態交換，嚴格限制於靜態配置的 12 Bytes 二進位合約（ABI），禁止任何指標傳遞或動態長度負載。

## 3. Rationale (決策理據)
強制將「邊界物理化」。透過預分配的極小靜態記憶體結構，確保跨環境記憶體佈局的絕對一致性，徹底消弭 User Space 資料污染 Kernel Space 的風險，並維持 O(1) 的極致存取速度。

## 4. Alternatives (替代方案)
* **Alternative A:** 採用 JSON 序列化格式傳遞感測狀態。
* **Alternative B:** 採用 Protobuf (Protocol Buffers) 等動態長度的二進位序列化框架。

## 5. Rejected Alternatives (拒絕原因)
* **Rejected Alternative A/B because:** 動態序列化格式會引入字串解析、動態記憶體配置（`kmalloc`/`kfree`），大幅增加 Kernel 記憶體碎片化（Fragmentation）風險。此舉將徹底破壞物理防禦層的決定性（Determinism）與執行效率，並引入非必要的序列化運算開銷。

## 6. Consequences (預期影響與取捨)
* **Positive:** 獲得了絕對的記憶體安全與微秒級的 IOCTL 存取效能，杜絕了所有 Payload 注入攻擊的可能性。
* **Negative:** 喪失了資料擴展彈性。未來若需新增感測器特徵，只要超過 12 Bytes 上限，就必須修改全域 ABI 合約、重新編譯所有軟硬體節點，或是採用多工（Multiplexing）機制導致 Context Switch 增加。

## 7. Related AKB (關聯架構知識庫)
* [`AKB / 03_ICD_Contract.md`](../AKB/03_ICD_Contract.md)
* [`AKB / 02_OT_Defense_Layer.md`](../AKB/02_OT_Defense_Layer.md)
* **Concept:** ABI Contract Boundary (合約邊界)

## 8. Evidence / References (證據與參考資料)
* 實作證據：[`v5_ioctl_contract.h`](../../ot_defense_layer/include/v5_ioctl_contract.h) 中強制宣告 `#pragma pack(1)` 取消記憶體對齊。
* 編譯期防護：使用 `static_assert(sizeof(...) == 12)`，確保檔案大小在編譯階段絕對吻合。