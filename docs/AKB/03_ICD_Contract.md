# 03_ICD_Contract.md

## 1. Identity & Purpose (身分與目的)

* **Module Name:** `v5_ioctl_contract` (跨環境通訊合約)
* **Architecture Level:** `Level 0.5 (Kernel/User Space ABI Boundary)`
* **Role:** Interface Control Document (ICD) 與靜態二進位邊界。
* **Core Responsibility:** 透過固定長度 (12 Bytes) 的靜態資料結構，定義 OT 防禦層 (Kernel Space) 與 IT 邊緣層 (User Space) 之間的唯一資料交換格式與狀態語意。

## 2. Architecture Boundary (架構邊界)

* **Inbound (接收):** `V5_IOC_EXCHANGE` IOCTL 系統呼叫指令與 User Space 記憶體位址。
* **Outbound (輸出):** 12 Bytes 的靜態物理狀態記憶體快照 (`v5_ioctl_contract_t`)。
* **Strict Constraints (嚴格限制):**
* **不包含動態大小：** 禁用任何記憶體指標 (Pointers) 與變長陣列 (VLA)。
* **不包含業務邏輯：** 結構體僅定義物理量測（如距離、溫度、系統層級、心跳），禁止引入應用層概念（如 `user_id`, `rfid_hash`, `door_request`）。



## 3. Validation Traceability (驗證溯源)

* **Item 1: 記憶體佈局一致性 (Memory Alignment Control)**
* **Evidence (客觀證據):** [`v5_ioctl_contract.h`](../../ot_defense_layer/include/v5_ioctl_contract.h) 中使用了 `#pragma pack(push, 1)`，並宣告 5 個明確長度的基本型別變數（`uint8_t` x2, `uint16_t` x1, `uint32_t` x2，總計 12 Bytes）。
* **Architecture Inference (架構推導):** 強制取消編譯器預設的位元組對齊 (Byte Padding)，確保跨硬體架構與跨編譯器環境下的記憶體佈局一致，建立 O(1) 複雜度的狀態解析機制。 Ref:[`ADR-0002`](../ADR/ADR-0002-Adopt_Fixed_12_Byte_ABI.md)


* **Item 2: 編譯期 ABI 防護 (Compile-time ABI Defense)**
* **Evidence (客觀證據):** 實作了 `#ifdef __KERNEL__` 條件編譯，並分別呼叫 `_Static_assert(sizeof(...) == 12)` 與 `static_assert(sizeof(...) == 12)`。
* **Architecture Inference (架構推導):** 透過編譯期斷言阻斷任何偏離 12 Bytes 的合約修改，確保執行期 (Runtime) 不會發生 ABI 解析錯位與記憶體越界。 Ref: [`ADR-0002`](../ADR/ADR-0002-Adopt_Fixed_12_Byte_ABI.md), [`ADR-0004`](../ADR/ADR-0004-Single_C_Header_Contract.md)


* **Item 3: 跨環境語意統一 (Semantic Unification)**
* **Evidence (客觀證據):** 檔案內統一定義了 `V5_STATE_NORMAL`, `V5_STATE_EMERGENCY`, `V5_FENCE_BRAKING` 等常數巨集。
* **Architecture Inference (架構推導):** 消除 IT 網域與 OT 網域對防禦狀態值的歧義，確立跨層級共同的領域語言 (Ubiquitous Language)，並限制底層上報的狀態種類。 Ref: [`ADR-0003`](../ADR/ADR-0003-Enforce_Stateless_Defense.md), [`ADR-0004`](../ADR/ADR-0004-Single_C_Header_Contract.md)



## 4. Future Evolution & Triggers (未來演進與觸發條件)

* **Evolution Item 1: 導入 ABI 版號管理機制 (ABI Versioning)**
* **Trigger:** 當系統需引入新種類的實體感測器（如震動、異常電壓監控），且要求舊版 IT 節點能向下相容解析新版 OT 快照時。
* **Expected Impact (預期影響):** 增加合約欄位（如新增 `uint8_t version`）與 Bridge Node 的解析邏輯複雜度；但可降低 IT 與 OT 節點必須強制同步部署與停機更新的營運成本。


* **Evolution Item 2: 多路多工合約 (Multiplexed Contract)**
* **Trigger:** 當感測器數量或種類超越單一輕量結構體的合理負荷，或導致 IOCTL 單次傳輸時間增加，影響底層輪詢頻率時。
* **Expected Impact (預期影響):** 將 `V5_IOC_EXCHANGE` 拆分為多個專用 IOCTL 指令（如環境專用、心跳專用）。此舉會增加 Kernel 驅動與 IT Gateway 的通訊次數（Context Switch 開銷），但能維持單一合約的精煉性與高頻取樣效率。



