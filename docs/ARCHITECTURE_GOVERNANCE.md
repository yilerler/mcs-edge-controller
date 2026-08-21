# MCS Edge Controller Architecture Governance

> *"The essence of software architecture lies not in the capabilities it grants the system, but in the constraints it imposes."*
> (軟體架構的本質，不在於它賦予了系統什麼能力，而在於它對系統施加了什麼約束。)

本文件為 MCS Edge Controller 的「架構治理法典」。
我們不將文件視為單純的「說明書」，而是將其視為一套 **「工件中介的架構治理系統 (Artifact-Mediated Architecture Governance)」**。本文件定義了系統中的知識如何被宣告、追蹤、驗證與修改，以確保架構的設計意圖與底層程式碼永遠保持一致。

---

## 1. 知識治理的三位一體 (The Artifact Triad)

本專案將架構知識嚴格拆分為三個互相約束的維度。任何架構的變更，都必須在這三個維度中達成邏輯閉環：

| Artifact (工件) | 核心提問 | 職責與性質 |
| :--- | :--- | :--- |
| **AKB (架構知識庫)** | **What is true now?** | **架構正典 (Canon)：** 定義系統當下的實體邊界、權力約束與單一真理來源 (SSOT)。 |
| **ADR (架構決策紀錄)** | **Why did we choose this?** | **決策脈絡 (Rationale)：** 紀錄架構演進的歷史，包含面臨的妥協、被拒絕的方案與預期後果。 |
| **Repository Evidence** | **Where is the proof?** | **物理現實 (Reality)：** 支撐架構斷言的實際程式碼、合約標頭檔 (`.h`) 與編譯期斷言。 |

---

## 2. 語意合約與視覺化基準 (The Semantic Contract)

我們揚棄傳統的「看圖猜意思」，採用**約束優先 (Constraint-First)** 的視覺化推導模型。系統中的任何架構圖表 (Canonical Diagram) 都不是隨意繪製的裝飾品，而是從底層約束推導而出的結果。

其推導鏈條如下：
`Term (語彙) ➔ Constraint (約束定義) ➔ Repository Evidence (程式碼證據) ➔ Diagram (視覺化關係)`

### 2.1 約束狀態標籤 (Constraint Status Tags)
為避免「圖畫得比程式碼更有信心」的文件債陷阱，AKB 中的所有架構約束與防禦機制，必須標註以下三種狀態之一：

* **`[Verified]` (已驗證):** 具備明確的程式碼證據支撐，且已實作卡點 (如 compile-time assertion 或架構攔截邏輯)。
* **`[Inferred]` (架構推論):** 邏輯上必然如此，但依賴外圍機制間接成立，缺乏直接的程式碼斷言保護。
* **`[Intended]` (架構意圖):** 由 ADR 決定的未來走向，目前程式碼尚未完全對齊，為下一階段的重構目標。

---

## 3. 架構變更的狀態機 (The Governance State Machine)

當系統需要引入改變邊界、修改權限或變更核心合約的新功能時，必須觸發以下治理流程 (Governance Loop)：

```text
  [1. 觸發變更] 
  Repository Evidence (程式碼/合約) 需要被修改
        │
        ▼
  [2. 提出決策] 
  撰寫 ADR (紀錄 Why / Trade-offs / Alternatives)
        │
        ▼
  [3. 宣告真理] 
  更新 AKB (重新定義 What / 修正 Canonical Diagram)
        │
        ▼
  [4. 執行對齊] 
  PR 提交，架構審查者核對 Evidence 與 Diagram 是否完美咬合

```

### 3.1 架構衝突裁量基準

當系統文件與程式碼發生矛盾時，依循以下最高法則進行除錯：

1. **Diagram 與 AKB 衝突：** Diagram 必須被修正（消除視覺幻覺）。
2. **Evidence (程式碼) 與 AKB 衝突，且無新 ADR：** 程式碼變更被視為「架構退化 (Regression)」，PR 必須被退回。
3. **Evidence (程式碼) 隨新 ADR 合法變更：** 舊的 AKB 與 Diagram 被標示為過期 (Stale)，必須立即觸發 AKB 的語意修補 (Semantic Patch)。

---

## 4. 運行期異常與架構對帳機制 (Runtime Anomaly & Reconciliation)

在本系統中，我們不將 Bug 視為單純的程式碼錯誤，而是將其視為**「實體現實與邏輯合約之間的對帳失敗 (Reconciliation Failure)」**。當系統在運行期 (Runtime) 遭遇崩潰、卡死或非預期行為時，必須依循以下架構邊界進行對帳：

### 4.1 邊界對帳三步驟 (The 3-Step Reconciliation)

當異常發生時，開發者必須停止在應用層 (L3) 盲目下斷點，改由底層向外盤點真理：

1. **L0 實體防線對帳 (The Physical Check):** 
   * 檢查 Kernel Log (`dmesg`)。
   * 若發現記憶體錯位或 `[WRITE REJECTED]` 以外的錯誤，代表 **12 Bytes ABI 剛性合約被破壞**。此為最高危險級別 (P0)，必須優先修復 `v5_elc.ko` 驅動或 `v5_interfaces` 型別。
2. **L1 語意大腦對帳 (The SSOT Check):**
   * 檢查 ROS 2 核心大腦 (`v5_core_bridge`) 的狀態機 (FSM) 日誌。
   * 比對「L0 回報的實體感測值」與「L1 FSM 當下的 State」。若兩者矛盾（例如：OT 已經斷電，但 L1 狀態仍顯示為 `NORMAL`），代表 **狀態機轉移矩陣 (State Transition Matrix) 出現死結或漏洞**。
3. **L3 無狀態微服務對帳 (The ACL Check):**
   * 若 L0 與 L1 狀態完美吻合，但特定微服務 (如戰情板或感測器轉接器) 數值異常，代表 **L2 防腐層 (ACL) 發生了語意污染或 Topic 阻塞**。
   * 處理方式：直接重啟該微服務。根據 `[ Zero Horizontal Coupling ]` 原則，任何 L3 節點的崩潰與重啟，絕對不允許影響 L0 與 L1 的運作。

> **治理守則：** 任何透過上述對帳流程抓出的架構性 Bug，在修復程式碼後，必須同步回頭檢視並更新 AKB 中的狀態機或合約定義，確保「文件正典」與「程式碼現實」再次咬合。


