# 03_ICD_Contract.md

## 1. Identity & Purpose (身分與目的)

* **Module Name:** `v5_ioctl_contract` (跨環境通訊合約)
* **Architecture Level:** `Level 0.5 (Kernel/User Space ABI Boundary)`
* **Role:** Interface Control Document (ICD) 與靜態二進位邊界。
* **Core Responsibility:** 透過固定長度 (12 Bytes) 的靜態資料結構，定義 OT 防禦層 (Kernel Space) 與 IT 邊緣層 (User Space) 之間的唯一資料交換格式與狀態語意。

## 2. Architecture Boundary (架構邊界)

本層級作為純靜態定義，不存在執行期實體，因此不具備傳統的資料收發流向 (Inbound/Outbound)。其邊界約束展現於對 IT 與 OT 兩端的**強制作業規範**：

* **載具定義 (Transport Carrier):** 僅允許透過單一窄橋 `V5_IOC_EXCHANGE` (基於 `_IOWR` 系統呼叫) 穿越 Kernel/User Space 邊界。
* **負載結構 (Payload Schema):** 強制鎖定為 12 Bytes 的物理狀態快照 (`v5_ioctl_contract_t`)。
* **Strict Constraints (嚴格限制):**
   * **零動態解析：** 禁用任何記憶體指標 (Pointers)、變長陣列 (VLA) 或字串解析，確保 O(1) 的空間與時間複雜度。
   * **零業務邏輯：** 結構體僅定義物理量測（如距離、溫度、系統層級、心跳），嚴禁引入應用層概念（如 `user_id`, `rfid_hash`, `door_request`）。



### 📊 Canonical Diagram: 12-Byte ICD & Constraints

本圖表呈現 Level 0.5 合約層的內部結構，以及其如何作為「單一真理來源 (SSOT)」，同時箝制 IT 與 OT 兩端的編譯與記憶體行為（參見 Section 3）。

```text
       [ IT DOMAIN (C++ / ROS 2 Gateway) ]
             │                       │
     (#include)                      (static_assert)
             │                       │
             ▼                       ▼
   +===================================================+
   |  Level 0.5: ABI Contract (Single Source of Truth) |
   |  [v5_ioctl_contract.h]                            |
   +---------------------------------------------------+
   | 📖 [ Shared Semantic Dictionary ]                 |
   |    V5_STATE_NORMAL = 0, V5_STATE_EMERGENCY = 3    |
   |    V5_FENCE_CLEAR = 0,  V5_FENCE_BRAKING = 1      |
   +---------------------------------------------------+
   | 📦 [ Static Memory Layout ]                       |
   |    #pragma pack(1)                                |
   |   ┌──────┬──────┬───────┬─────────┬─────────┐     |
   |   │ Lv(1)│Sts(1)│Heat(2)│ Dist(4) │Hbeat(4) │=12B |
   |   └──────┴──────┴───────┴─────────┴─────────┘     |
   +---------------------------------------------------+
   | 🌉 [ Narrow Bridge Definition ]                   |
   |    V5_IOC_EXCHANGE (_IOWR)                        |
   +===================================================+
             ▲                       ▲
             │                       │
     (#include)                      (_Static_assert)
             │                       │
       [ OT DOMAIN (C / Linux Kernel Module) ]

```

> **Visual Semantics (視覺語意與證據綁定)**
> * `(#include) / SSOT`: 象徵跨環境共用單一檔案。對應 `Item 3`，由標頭檔內的 `#ifdef __KERNEL__` 跨環境支援提供證據。
> * `(Assert) / 12B`: 象徵編譯期 ABI 防護。對應 `Item 2`，由雙環境下的 `sizeof(...) == 12` 斷言提供證據。
> * `#pragma pack(1)`: 象徵記憶體對齊控制。對應 `Item 1`，確保 12 Bytes 在跨硬體/編譯器下佈局絕對一致。
> 
> 

---

## 3. Validation Traceability (驗證溯源)

* **Item 1: 記憶體佈局一致性 (Memory Alignment Control)**
  * **Evidence (客觀證據):** [`v5_ioctl_contract.h`](../../ot_defense_layer/include/v5_ioctl_contract.h) 中明確使用了 `#pragma pack(push, 1)`，並宣告 5 個明確長度的基本型別變數（`uint8_t` x2, `uint16_t` x1, `uint32_t` x2，總計 12 Bytes）。
  * **Architecture Inference (架構推導):** 強制取消編譯器預設的位元組對齊 (Byte Padding)，確保跨硬體架構與跨編譯器環境下的記憶體佈局絕對一致，建立無解析開銷 (O(1) 複雜度) 的實體狀態傳輸機制。 Ref: [`ADR-0002`](../ADR/ADR-0002-Adopt_Fixed_12_Byte_ABI.md)


* **Item 2: 編譯期 ABI 防護 (Compile-time ABI Defense)**
	* **Evidence (客觀證據):** 實作了 `#ifdef __KERNEL__` 條件編譯，並分別針對 C++ (IT 網域) 呼叫 `static_assert(sizeof(...) == 12)` 與 C (OT 網域) 呼叫 `_Static_assert(sizeof(...) == 12)`。
	* **Architecture Inference (架構推導):** 透過強硬的編譯期斷言，阻斷任何偏離 12 Bytes 的合約修改（如人為新增欄位卻未雙邊同步）。確保執行期 (Runtime) 絕對不會發生 ABI 解析錯位與記憶體越界，將致命錯誤攔截在部署之前。 Ref: [`ADR-0002`](../ADR/ADR-0002-Adopt_Fixed_12_Byte_ABI.md), [`ADR-0004`](../ADR/ADR-0004-Single_C_Header_Contract.md)


* **Item 3: 跨環境語意統一 (Semantic Unification)**
	* **Evidence (客觀證據):** 檔案內統一定義了 `V5_STATE_NORMAL`, `V5_STATE_EMERGENCY`, `V5_FENCE_BRAKING` 等常數巨集，並供上下兩層共用。
	* **Architecture Inference (架構推導):** 將 `v5_ioctl_contract.h` 確立為 IT 網域與 OT 網域的「單點真理 (Single Source of Truth)」。消除跨層級對防禦狀態值的歧義，確立了共同的領域語言 (Ubiquitous Language)，並從源頭限制了底層上報的狀態種類。 Ref: [`ADR-0003`](../ADR/ADR-0003-Enforce_Stateless_Defense.md), [`ADR-0004`](../ADR/ADR-0004-Single_C_Header_Contract.md)



## 4. Future Evolution & Triggers (未來演進與觸發條件)

* **Evolution Item 1: 導入 ABI 版號管理機制 (ABI Versioning)**
	* **Trigger:** 當系統需引入新種類的實體感測器（如震動、異常電壓監控），且要求舊版 IT 節點能向下相容解析新版 OT 快照時。
	* **Expected Impact (預期影響):** 增加合約欄位（如新增 `uint8_t version`）與 Bridge Node 的解析邏輯複雜度；但可降低 IT 與 OT 節點必須強制同步部署與停機更新的營運成本。


* **Evolution Item 2: 多路多工合約 (Multiplexed Contract)**
	* **Trigger:** 當感測器數量或種類超越單一輕量結構體的合理負荷，或導致 IOCTL 單次傳輸時間增加，影響底層輪詢頻率時。
	* **Expected Impact (預期影響):** 將單一 `V5_IOC_EXCHANGE` 窄橋拆分為多個專用 IOCTL 指令（如環境專用、心跳專用）。此舉會增加 Kernel 驅動與 IT Gateway 的通訊次數（Context Switch 開銷），但能維持單一合約的精煉性與高頻取樣效率。


