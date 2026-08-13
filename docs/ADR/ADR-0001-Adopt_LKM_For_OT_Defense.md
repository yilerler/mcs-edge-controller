# ADR-0001: 採用 Linux Kernel Module (LKM) 實作 OT 防禦層

* **ID:** ADR-0001
* **Date:** 2026-08-01
* **Status:** Accepted
* **Custodian:** `ot_defense_layer`

---

## 1. Context / Problem (背景與問題)

在混合臨界系統（Mixed Criticality System）中，IT 網域的邊緣運算層（如 ROS 2 節點、外部網路通訊）面臨著非決定性的風險，包含網路延遲、微服務記憶體洩漏（Memory Leak）或框架崩潰（Crash）。若將底層硬體感測器與致動器的控制邏輯交由 IT 網域直接處理，一旦 IT 發生異常，系統將連帶喪失最基礎的物理工安防護能力（例如大門無法強制鎖死、無法偵測人員闖入）。我們需要一道絕對的實體與邏輯隔離屏障。

## 2. Decision (決策內容)

採用 Linux Kernel Module (LKM) 作為 OT 防禦層的核心實作技術，將實體硬體狀態的維護與底層安全防禦機制，完全限制在 Kernel Space 內執行。

## 3. Rationale (決策理據)

確保核心的記憶體空間與 User Space 徹底物理隔離，使得感測器讀取與物理防護機制的運作，絕對不受 ROS 2 節點崩潰、User Space 資源耗盡或網路通訊延遲的干擾。這確立了「物理限制先於邏輯防護」的架構底線。

## 4. Alternatives (替代方案)

* **Alternative A:** 將硬體控制邏輯移入 User Space，透過 Python 或 C++ (如 `libgpiod` 函式庫或讀取 `/sys/class/gpio` 等 `sysfs` 節點) 直接在 ROS 2 微服務節點內進行輪詢與讀取。

## 5. Rejected Alternatives (拒絕原因)

* **Rejected Alternative A because:** User Space 的行程排程（Process Scheduling）受 IT 系統整體負載影響極大，無法保證微秒/毫秒級的決定性硬體輪詢週期（Determinism）。此外，若 ROS 2 框架或其依賴的底層中介軟體（DDS）發生異常，系統將連帶且徹底喪失基礎實體防護能力，引發工安危機。

## 6. Consequences (預期影響與取捨)

* **Positive:** 獲得了極高的系統強韌度與邊緣主權。即使 User Space 全面崩潰，Kernel Space 仍能獨立判斷物理邊界（如距離過近）並自主執行硬體級的安全降級（如觸發煞車或鎖死大門）。
* **Negative:** 大幅增加了底層驅動開發與除錯的複雜度；若 LKM 內部發生指標錯誤，將導致嚴重的 Kernel Panic 使整台設備死機。同時，未來若需新增或修改實體感測器的行為邏輯，必須重新編譯核心模組並重新掛載，缺乏動態擴展彈性。

## 7. Related AKB (關聯架構知識庫)

* [`AKB / 02_OT_Defense_Layer.md`](../AKB/02_OT_Defense_Layer.md)
* [`AKB / 01_Architecture_Glossary.md`](../AKB/01_Architecture_Glossary.md)
* **Concept:** Single Source of Truth (單一真理來源)、Hardware Abstraction (硬體抽象)

## 8. Evidence / References (證據與參考資料)

* 實作證據：[`mock_elc_core.c`](../../ot_defense_layer/src/mock_elc_core.c)
* 使用 `<linux/module.h>` 與 Spinlock 機制確保狀態獨立性。