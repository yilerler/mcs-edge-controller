# 02_OT_Defense_Layer.md
## 1. Identity & Purpose (身分與目的)

* **Module Name:** `ot_defense_layer`
* **Architecture Level:** `Level 0 (Hardware/Kernel Interface & Absolute Physical Boundary)`
* **Role:** Linux Loadable Kernel Module (LKM) 實體狀態控制器。
* **Core Responsibility:** 透過 12 Bytes 靜態記憶體映射實體感測狀態，隔離 IT 網域非決定性狀態，維持獨立的物理防護運作。

## 2. Architecture Boundary (架構邊界)

* **Inbound (接收):** 實體感測器訊號（透過 Kernel Thread 輪詢模擬）與硬體中斷。
* **Outbound (輸出):** 12 Bytes ABI 靜態結構，透過 `V5_IOC_EXCHANGE` IOCTL 提供給北向的 `v5_core_bridge`。
* **Strict Constraints (嚴格限制):**
* **不承載業務邏輯：** 卸除所有存取控制（如門禁判定、RFID 驗證），相關邏輯全數移交 IT 層。
* **不主動發起通訊：** 採用被動輪詢（Polling/IOCTL）架構，不向 User Space 注入訊號或回呼（Callback）。
* **不使用動態記憶體：** 與 IT 的狀態交換限制於靜態配置的 12 Bytes 結構，避免指標溢位與記憶體外洩。



## 3. Validation Traceability (驗證溯源)

* **Item 1: 實體狀態的單一真理來源**
* **Evidence (客觀證據):**
* 目錄 `ot_defense_layer/` 與 `Makefile` 獨立於 ROS 2 工作區。
* [`mock_elc_core.c`](../../ot_defense_layer/src/mock_elc_core.c) 中使用了 `<linux/spinlock.h>` 與 `spin_lock_irqsave()` 保護狀態更新。


* **Architecture Inference (架構推導):** 透過獨立的編譯流程與 Spinlock 機制，在 Kernel Space 確立了實體狀態的記憶體唯一性，並與 IT 部署期及執行期完成物理隔離，免除 Race Condition 風險。 Ref: [`ADR-0001`](../ADR/ADR-0001-Adopt_LKM_For_OT_Defense.md)


* **Item 2: 合約邊界控制**
* **Evidence (客觀證據):**
* `v5_ioctl_contract.h` 中包含 `#pragma pack(1)` 與 `static_assert(sizeof(...) == 12)`。
* [`mock_elc_core.c`](../../ot_defense_layer/src/mock_elc_core.c) 的 `elc_ioctl` 實作僅執行 `copy_to_user` 與 `copy_from_user`。


* **Architecture Inference (架構推導):** 藉由編譯期斷言確保跨環境 ABI 記憶體佈局一致；IOCTL 處理常式無條件分支（If-else）與身分識別欄位，證明已成功將業務邏輯從核心態卸載。 Ref: [`ADR-0002`](../ADR/ADR-0002-Adopt_Fixed_12_Byte_ABI.md), [`ADR-0003`](../ADR/ADR-0003-Enforce_Stateless_Defense.md)


* **Item 3: 自主安全降級**
* **Evidence (客觀證據):** [`mock_elc_core.c`](../../ot_defense_layer/src/mock_elc_core.c) 的輪詢執行緒內具備 `if (mock_distance < CRITICAL_DISTANCE_MM) { ... = V5_FENCE_BRAKING; }` 賦值邏輯。
* **Architecture Inference (架構推導):** 確立了 OT 層在不依賴 IT 網路或 ROS 2 節點的條件下，具備自主切換實體安全狀態防護的控制權。 Ref: [`ADR-0001`](../ADR/ADR-0001-Adopt_LKM_For_OT_Defense.md), [`ADR-0003`](../ADR/ADR-0003-Enforce_Stateless_Defense.md)



## 4. Future Evolution & Triggers (未來演進與觸發條件)

* **Evolution Item 1: 導入 PREEMPT_RT 補丁**
* **Trigger:** 當標準 Linux 核心排程器（Standard Kernel Scheduler）的 Context Switch 導致 `usleep_range` 產生不可預測的 Jitter，且該延遲導致 IT 服務無法滿足微秒級（us）硬即時（Hard Real-time）規範時。
* **Expected Impact (預期影響):** 提升微秒級即時性的保證，但將增加 Kernel 維護與編譯成本，且可能略微降低系統整體的總吞吐量（Throughput）。


* **Evolution Item 2: 轉移核心輪詢邏輯至獨立 RTOS MCU (如 Raspberry Pi Pico)**
* **Trigger:** 當系統需要與更高安全等級（如 SIL-2/SIL-3）的工業控制設備對接，且 LKM 仍有牽連作業系統崩潰（Kernel Panic）的風險無法被完全容忍時。
* **Expected Impact (預期影響):** 達成真正的硬體級決定性防護與實體隔離，但將增加硬體 BOM 成本（需額外配置 MCU），並引入全新的跨硬體通訊與除錯複雜度（如 SPI/UART 通訊處理）。


