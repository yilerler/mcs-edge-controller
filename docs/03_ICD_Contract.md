# 跨環境通訊合約 (Interface Control Document, ICD)

## 系統定位 (Verified Narrative)
本模組（`v5_ioctl_contract.h`）是邊緣控制器中，OT 防禦層 (Kernel Space) 與 IT 邊緣層 (User Space) 之間的**單點真理 (Single Source of Truth)** 與唯一合法的二進位邊界 (ABI)。
它成功實現了 IT/OT 的邏輯解耦，透過極度精煉且固定長度的資料結構，徹底剝離了如門禁判定、身份驗證等非決定性業務邏輯，確保 OT 核心退守至最純粹的物理狀態防護。

---

## 1. Architecture (架構意圖與邊界)

### 存在目的 (Purpose)
在作業系統層級建立一道物理與邏輯的防護邊界，將 IT 側的不可預期性（如網路請求延遲、複雜的存取控制）完全阻擋在 OT 防禦層之外，只允許純粹的「物理狀態快照」進行跨環境交換。

### 互動關係 (Relationships)
本合約是系統跨環境通訊的絕對樞紐：
*   **OT 驅動程式 (`mock_elc_core.c`)** 依賴此合約生成實體狀態快照。
*   **IT 閘道器 (`Core Bridge`)** 依賴此合約進行狀態的讀取與適配。
兩者僅透過 `V5_IOC_EXCHANGE` 作為唯一合法的 IOCTL 資料交換通道。

### ⛔ 絕對邊界 (Architecture Boundaries)
為確保防禦縱深，本合約設計實作了極致的戰略收斂，嚴格遵守以下禁區：
*   **絕對不承載動態大小：** 合約內禁用任何指標 (Pointers) 或變長陣列 (VLA)，從根本上根除指標越界與跨環境記憶體外洩的穿透風險。
*   **絕對不承載業務邏輯：** 合約欄位僅聚焦於物理量測（如距離、溫度、硬體時序心跳）。結構體中絕對不存在 `user_id`、`rfid_hash` 或 `door_open_request` 等應用層概念。

---

## 2. Implementation (實作機制與客觀證據)

本合約的架構宣稱，由以下 C 語言編譯期特性與記憶體控制機制強制保證：

*   **記憶體佈局絕對一致性 (Memory Alignment Control)：**
    透過 `#pragma pack(push, 1)` 強制取消編譯器的預設位元組對齊 (Byte Padding)，精準將 `v5_ioctl_contract_t` 控制在 12 Bytes。這確保了跨硬體架構、跨編譯器環境下的記憶體佈局絕對一致。
*   **編譯期的 ABI 防護 (Compile-time ABI Defense)：**
    實作了跨 Kernel 與 User Space 雙環境的靜態斷言 (`_Static_assert` 與 `static_assert`)。任何企圖單方面修改合約欄位，導致資料長度偏離 12 Bytes 的破壞架構行為，都會在**編譯階段 (Compile-time) 被系統強制阻斷**，連執行檔都無法生成。
*   **跨環境語意統一 (Semantic Unification)：**
    透過 `#ifdef __KERNEL__` 進行環境分支，確保同一份標頭檔可同時於驅動程式與應用程式中引入，並統一定義了 `V5_STATE_EMERGENCY` 等巨集，消除 IT/OT 兩端對「緊急狀態」的各自表述。

---

## 3. Engineering Assessment (工程限制與設計取捨)

> **架構取捨決策 (Architecture Decisions & Trade-offs)**

*   **Decision (決策):** 採用嚴格的 12 Bytes 固定長度 (Fixed-size) 二進位合約。
*   **Constraint (限制):** 系統喪失了 Payload 的動態擴展性 (Dynamic Schema)。目前不支援動態增減感測器欄位或傳輸可變長度的字串訊息。
*   **Trade-off (取捨):** 為了換取 **「極致的記憶體安全」**與 **「O(1) 複雜度的狀態解析」**，我們放棄了 JSON 或 Protobuf 等具備高度擴展性的序列化方案。若未來需要新增實體感測器，必須修改此合約，並**同步重新編譯 Kernel Module 與 User Space Node**。在 Mixed Criticality System 中，這種高維護成本是為了保證底層穩定性所做出的理性工程取捨。

---

## 4. Future Evolution (未來演進方向)

基於上述固定合約的限制，當未來系統需外接更多種類的實體感測器或進行大規模部署時，架構演進方向將評估：
1.  **ABI 版號管理機制 (ABI Versioning)：** 在合約頭部引入固定的 `uint8_t version` 欄位，讓 IT 層的 Bridge 能向下相容並識別不同版本的 OT 防禦層快照。
2.  **多路多工合約 (Multiplexed Contract)：** 若感測器數量超過單一輕量結構體的合理負荷，將評估拆分 `V5_IOC_EXCHANGE` 指令，將環境感測與硬體心跳拆分為獨立的定長合約通道，以維持單一合約的精煉性。