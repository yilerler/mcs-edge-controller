
# OT Defense Layer (操作科技底層防禦子系統)

## 系統定位 (Verified Narrative)
本模組實作為獨立的 Linux 核心模組（Loadable Kernel Module, LKM），以系統底層驅動的姿態運行於 Kernel Space 。
其架構本質是一台純粹的「物理狀態快照機」，透過強制性的 12 Bytes 靜態記憶體合約（Contract），徹底剝離了門禁驗證等高階業務邏輯 。此架構確保了 OT 層在面對 IT 服務的狀態爆炸或崩潰時，仍能維持獨立、安全的物理防護運作 。

---

## 1. Architecture (架構意圖與邊界)

### 存在目的 (Purpose)
建立一道絕對的物理與邏輯邊界，將底層硬體的決定性執行語意，與上層 IT 應用服務的非決定性狀態（如網路延遲、複雜業務邏輯）徹底隔離，從根本上防堵系統狀態爆炸造成的工安風險 。

### 互動關係 (Relationships)
*   **Northbound（北向）：** 僅透過單一介面 `V5_IOC_EXCHANGE` 與 IT 層（具體為 ROS 2 的 `v5_core_bridge`）建立依賴 。OT 層對 IT 層內部的微服務架構（Gateway、環境監測、門禁服務等）保持**完全無知（Zero Knowledge）** 。
*   **Southbound（南向）：** 直接面對實體硬體與感測器（目前由 Kernel Thread 與 Sysfs 節點模擬物理中斷與感測資料流） 。

### ⛔ 絕對邊界 (Architecture Boundaries)
本模組嚴格遵守以下禁區，任何跨越邊界的更動都將被視為架構退化（Regression）：
*   **絕對不承載業務邏輯：** 徹底拋棄存取控制（如門禁判定、RFID 驗證），所有與「人」或「業務」相關的邏輯全數推至 IT 層處理 。
*   **絕對不主動發起通訊：** 採用被動輪詢（Polling/IOCTL）架構，不向 User Space 注入不可預期的訊號或回呼（Callback） 。
*   **絕對不使用動態記憶體：** 與 IT 的狀態交換嚴格限制在靜態配置的 12 Bytes 結構內，阻斷任何指標溢位與記憶體外洩的穿透風險 。

---

## 2. Implementation (實作機制與客觀證據)

本模組的架構宣稱均具備以下實體機制與 Repository 證據支撐：

*   **物理狀態的唯一真理來源 (Single Source of Truth)：**
    負責直接採集、維護與鎖定（透過 Spinlock）最原始的實體感測數據（如超音波距離、火警溫度） 。在 Repository 中，實體的資料夾結構與 Makefile 完全分離，證明了編譯期與部署期的物理隔離 。
*   **合約守門員 (Contract Gatekeeper)：**
    嚴格執行 12 Bytes 的 ABI 合約 。這由 `v5_ioctl_contract.h` 中的 `#pragma pack(1)` 與 `static_assert` 提供了 ABI 邊界的強硬控制證據 。同時，`mock_elc_core.c` 中的 `elc_ioctl` 實作僅包含單純的 `copy_to_user` 與 `copy_from_user`，未見任何業務條件判斷式 。
*   **自主安全降級 (Local Safety Fallback)：**
    當偵測到實體數值越界（如距離 < 500mm 或硬體觸發 `V5_STATE_EMERGENCY`）時，具備不依賴 IT 層即可自行切換安全狀態的控制權 。

---

## 3. Engineering Assessment (工程限制與真實性揭露)

> **已知架構缺口 (Known Architecture Gaps)**

目前的 OT Defense Layer 雖然在記憶體與業務邏輯上做到了隔離，但仍依賴標準 Linux 核心排程器（Standard Kernel Scheduler） 。
其輪詢機制（如 `usleep_range`）受制於系統 Context Switch 與總體負載，目前無法提供微秒級（us）的**硬即時（Hard Real-time）決定性保證** 。這意味著若發生極端的高頻物理中斷，系統回應可能會存在 Jitter（抖動） 。

---

## 4. Future Evolution (未來演進方向)

基於上述 Type C 限制與風險揭露，未來的架構迭代將朝以下方向評估以補齊行為驗證的拼圖：
1.  導入 **PREEMPT_RT** 補丁，提升 Linux 核心的即時搶佔能力 。
2.  將核心輪詢邏輯下放至獨立的 **RTOS 微控制器（如 Raspberry Pi Pico / MCU）**，以達到真正的硬體級決定性防護 。
