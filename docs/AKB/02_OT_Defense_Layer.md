# 02_OT_Defense_Layer.md

## 1. Identity & Purpose (身分與目的)

* **Module Name:** `ot_defense_layer`
* **Architecture Level:** `Level 0 (Hardware/Kernel Interface & Absolute Physical Boundary)`
* **Role:** Linux Loadable Kernel Module (LKM) 實體狀態控制器。
* **Core Responsibility:** 透過 12 Bytes 靜態記憶體映射實體感測狀態，隔離 IT 網域非決定性狀態，維持獨立的物理防護運作。

## 2. Architecture Boundary (架構邊界)

* **Inbound (接收):** 實體感測器硬體訊號與中斷。**嚴格禁止來自 IT 網域的下行業務狀態覆寫。**
* **Outbound (輸出):** 12 Bytes ABI 靜態結構，透過 `V5_IOC_EXCHANGE` IOCTL 提供給北向的 IT 閘道器進行物理狀態快照拉取 (Polling)。
* **Strict Constraints (嚴格限制):**
* **絕對唯讀防護 (Absolute Read-Only Boundary):** 雖然底層 IOCTL 遵循 `_IOWR` 系統呼叫標準接收 User Space 的記憶體載荷，但在 Kernel 內部會無條件捨棄並覆寫 IT 的下行資料。IT 網域對 OT 實體狀態擁有 0% 的寫入與決策權力。
* **不承載業務邏輯：** 卸除所有存取控制（如門禁判定、RFID 驗證），相關邏輯全數移交 IT 層。
* **不主動發起通訊：** 採用被動輪詢（Polling/IOCTL）架構，不向 User Space 注入訊號或回呼（Callback）。
* **不使用動態記憶體：** 與 IT 的狀態交換限制於靜態配置的 12 Bytes 結構，避免指標溢位與記憶體外洩。



### 📊 Canonical Diagram: OT/IT Physical Boundary

本圖表為實體防禦邊界的規範性視覺化模型。圖中所繪製的邊界約束與資料流向，皆與底層程式碼具備絕對的溯源綁定（參見 Section 3）。

```text
               [ IT DOMAIN (Non-Deterministic User Space) ]
                      (Level 2: v5_ot_gateway)
                                │
                                │ (V5_IOC_EXCHANGE Polling)
                                │
   [ WRITE REJECTED ]           ▼        [ READ PERMITTED ]
   X════════════════════════════╬═══════════════════════════════^
   X    [ Level 0.5: ABI Contract Boundary (12 Bytes Static) ]  ║
   X════════════════════════════╬═══════════════════════════════║
                                │ (IT Payload      (12 Bytes Physical
                                │  Overwritten)     State Snapshot)
                                ▼                           ║
             ┌──────────────────────────────────────────────╫─┐
             │    OT DOMAIN (Deterministic Kernel Space)    ║ │
             │                                              ║ │
             │   +======================================+   ║ │
             │   | [ Thread-Safe State Memory ]         |═══╝ │
             │   | (Absolute Atomicity / Mutual Excl.)  |     │
             │   +======================================+     │
             │         ▲                      ▲               │
             │         │                      │               │
             │ (State Update)           (State Update)        │
             │         │                      │               │
             │ ┌───────┴───────┐      ┌───────┴───────┐       │
             │ │ Hardware      │      │ Hardware IRQ  │       │
             │ │ Polling Thread│      │ Receiver      │       │
             │ │ (~20Hz)       │      │               │       │
             │ └───────┬───────┘      └───────┬───────┘       │
             │    ▲  │   (Unconditional       ▲               │
             │    │  │    Parallel Autonomy)  │               │
             └────┼──┼────────────────────────┼───────────────┘
                  │  │                        │
                  │  │ (Closed Safety Loop)   │ (Physical Interrupt)
                  │  ▼                        │
               [ Physical Sensors / Actuators / Hardware ]

```

> **Visual Semantics (視覺語意與證據綁定)**
> * `X══╬══^` **(Write Interception):** 象徵絕對唯讀邊界。對應 `Item 2`，由程式碼中強制丟棄 `copy_from_user` 負載提供證據。
> * `+===+` **(Mutex Region):** 象徵記憶體原子性防護區塊。對應 `Item 1`，由實作層的 Spinlock 機制確保跨環境讀取的一致性。
> * `▲ │ ▼` **(Closed Safety Loop):** 象徵實體平行防禦迴圈。對應 `Item 3`，由底層獨立運行的 Kernel Polling Thread 與實體感測器的自我閉環提供證據，證明其決策不依賴 IT 網域。
> 
> 

---

## 3. Validation Traceability (驗證溯源)

* **Item 1: 實體狀態的單一真理來源與原子性 (Memory Atomicity)**
* **Evidence (客觀證據):**
* 目錄 `ot_defense_layer/` 與 `Makefile` 獨立於 ROS 2 工作區。
* [`mock_elc_core.c`](../../ot_defense_layer/src/mock_elc_core.c) 在所有跨執行緒存取 `reg_map` 時，皆強制使用了 `<linux/spinlock.h>` 與 `spin_lock_irqsave()` 建立臨界區段 (Critical Section)。


* **Architecture Inference (架構推導):** 確保跨環境邊界讀取的**記憶體絕對原子性 (Atomicity) 與互斥性 (Mutual Exclusion)**。IT 網域在任何時刻（包含多執行緒併發輪詢），都只能拉取到完整一致的 12 Bytes 狀態快照，徹底免除跨環境通訊的資料破裂 (Torn Read) 與競態條件風險。 Ref: [`ADR-0001`](../ADR/ADR-0001-Adopt_LKM_For_OT_Defense.md)


* **Item 2: 合約邊界控制與寫入阻斷 (Write Interception)**
* **Evidence (客觀證據):**
* `v5_ioctl_contract.h` 中包含 `#pragma pack(1)` 與 `static_assert(sizeof(...) == 12)`。
* [`mock_elc_core.c`](../../ot_defense_layer/src/mock_elc_core.c) 的 `elc_ioctl` 實作中，明確使用 `local_copy = my_elc.reg_map;` 直接覆蓋掉由 `copy_from_user` 傳入的 IT 負載，無任何 `if-else` 身分識別分支。


* **Architecture Inference (架構推導):** 藉由編譯期斷言確保跨環境 ABI 記憶體佈局一致；且透過強制的記憶體覆寫，證明已成功將業務邏輯從核心態卸載，確立了實體邊界的**絕對唯讀 (Read-Only) 特性**。 Ref: [`ADR-0002`](../ADR/ADR-0002-Adopt_Fixed_12_Byte_ABI.md), [`ADR-0003`](../ADR/ADR-0003-Enforce_Stateless_Defense.md)


* **Item 3: 實體平行的無條件自治 (Unconditional Parallel Autonomy)**
* **Evidence (客觀證據):** [`mock_elc_core.c`](../../ot_defense_layer/src/mock_elc_core.c) 的輪詢執行緒 (`fence_polling_thread`) 是一個獨立以 ~20Hz 運行的 Kernel 迴圈，其 `if (mock_distance < CRITICAL_DISTANCE_MM) { ... = V5_FENCE_BRAKING; }` 的賦值判定並不依賴、也不檢查任何 IT 狀態。
* **Architecture Inference (架構推導):** 確立了 OT 層的**絕對硬體優先權 (Hardware Override)**。OT 的安全防護是平行且無條件的，只要物理閾值被突破，OT 層將常態性地自主執行安全降級。這並非 IT 斷線時的備援機制 (Fallback)，而是無論 IT 下達何種指令，OT 都能予以推翻 (Override) 的底層主權。 Ref: [`ADR-0001`](../ADR/ADR-0001-Adopt_LKM_For_OT_Defense.md), [`ADR-0003`](../ADR/ADR-0003-Enforce_Stateless_Defense.md)



## 4. Future Evolution & Triggers (未來演進與觸發條件)

* **Evolution Item 1: 導入 PREEMPT_RT 補丁**
* **Trigger:** 當標準 Linux 核心排程器（Standard Kernel Scheduler）的 Context Switch 導致 `usleep_range` 產生不可預測的 Jitter，且該延遲導致 IT 服務無法滿足微秒級（us）硬即時（Hard Real-time）規範時。
* **Expected Impact (預期影響):** 提升微秒級即時性的保證，但將增加 Kernel 維護與編譯成本，且可能略微降低系統整體的總吞吐量（Throughput）。


* **Evolution Item 2: 轉移核心輪詢邏輯至獨立 RTOS MCU (如 Raspberry Pi Pico)**
* **Trigger:** 當系統需要與更高安全等級（如 SIL-2/SIL-3）的工業控制設備對接，且 LKM 仍有牽連作業系統崩潰（Kernel Panic）的風險無法被完全容忍時。
* **Expected Impact (預期影響):** 達成真正的硬體級決定性防護與實體隔離，但將增加硬體 BOM 成本（需額外配置 MCU），並引入全新的跨硬體通訊與除錯複雜度（如 SPI/UART 通訊處理）。


