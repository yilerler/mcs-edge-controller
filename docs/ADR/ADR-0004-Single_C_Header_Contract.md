# ADR-0004: 將通訊合約定義為單一 C 標頭檔 (Single Header Contract)

* **ID:** ADR-0004
* **Date:** 2026-08-01
* **Status:** Accepted
* **Custodian:** `v5_ioctl_contract`

---

## 1. Context / Problem (背景與問題)

在混合 criticality 系統中，底層 OT（Kernel Space）與上層 IT（User Space）必須頻繁交換物理狀態快照。如果 IT 網域（如 ROS 2 閘道器）與 OT 網域（LKM 驅動）各自在自己的專案目錄下維護一份資料結構的定義檔，極易因為人為疏忽或編譯器差異，導致欄位型別長度不匹配（例如 32-bit 與 64-bit 整數的差異，或是隱式 Byte Padding 的錯位），這種微小的落差將在執行期（Runtime）引發嚴重的記憶體越界與核心崩潰（Kernel Panic）。

## 2. Decision (決策內容)

將跨環境通訊合約（ICD）強制定義為「單一 C 標頭檔 (`v5_ioctl_contract.h`)」，並要求 Kernel 驅動與 IT 網域的 C++ Gateway 節點，在編譯時都必須 `#include` 這一份絕對唯一的實體檔案。

## 3. Rationale (決策理據)

建立 IT 與 OT 間的「單點真理 (Single Source of Truth)」。透過共享單一標頭檔，確保兩端編譯基準完全一致。此外，將 `V5_STATE_NORMAL` 等常數巨集統一定義於此，消除了跨層級間對防禦狀態值的語意歧義，確立了共同的領域語言 (Ubiquitous Language)。

## 4. Alternatives (替代方案)

* **Alternative A:** 在 Kernel 驅動與 ROS 2 網域各自維護一份等效的資料結構定義檔案（例如在 Python 中用 `ctypes.Struct` 重新刻畫一次，或在 C++ 專案中再寫一個 struct）。

## 5. Rejected Alternatives (拒絕原因)

* **Rejected Alternative A because:** 依賴「人為紀律」來維持兩份合約的同步是極度脆弱的。任何版本脫鉤或編譯器對齊行為的差異，都會導致 IOCTL 讀取到錯位的記憶體區塊，引發災難性的系統崩潰。

## 6. Consequences (預期影響與取捨)

* **Positive:** 配合 `static_assert` 編譯期防護，徹底消滅了 ABI 錯位的執行期 Bug，讓通訊合約具備「編譯不過就無法執行」的絕對強健性。
* **Negative:** 形成了強烈的源碼級相依性（Source Code Dependency）。未來只要對這份 12 Bytes 合約進行任何微小的擴充或修改，都必須同時強制重新編譯並部署 Kernel Module 與 ROS 2 Gateway 節點，喪失了局部更新的彈性。

## 7. Related AKB (關聯架構知識庫)

* [`AKB / 03_ICD_Contract.md`](../AKB/03_ICD_Contract.md)
* [`AKB / 02_OT_Defense_Layer.md`](../AKB/02_OT_Defense_Layer.md)
* **Concept:** Single Source of Truth (單一真理來源)、Interface Control Document (ICD)

## 8. Evidence / References (證據與參考資料)

* 實作證據：[`v5_ioctl_contract.h`](../../ot_defense_layer/include/v5_ioctl_contract.h) 內部巧妙運用了 `#ifdef __KERNEL__` 條件編譯，讓同一份檔案能在 Kernel Space 使用 `_Static_assert`，而在 User Space 使用 `static_assert`，證明其被設計為跨環境共用的單一實體。