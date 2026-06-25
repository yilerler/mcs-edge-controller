# V5 MCS Edge Controller
**IT/OT 解耦防禦框架與 SIL (Software-In-The-Loop) 參考實作**

## 1. 專案本質 (What is this?)

本專案是一個專為 **混合關鍵場域 (Mixed-Criticality System, MCS)** 設計的 ROS 2 邊緣閘道器架構。

在傳統的工業物聯網與邊緣運算中，實體物理硬體（OT）與上層商業邏輯大腦（IT）往往高度耦合，導致網路的微小延遲容易拖垮底層硬體，而雲端指令的錯誤也常引發實體災難（腦裂危機）。

為了解決此痛點，本 Repo 提供了一套 **軟體迴圈 (SIL) 概念驗證實作**。我們透過在 Linux 核心空間 (Kernel Space) 與使用者空間 (User Space) 之間砸下嚴格的 **24 Bytes 數位合約 (ICD)**，徹底隔離實體硬體的混沌與上層的商業決策大腦。這套架構讓開發者可以在沒有實體感測器的情況下，直接透過內建的混沌模組，驗證系統在極端物理狀態下的防禦極限。

---

## 2. 核心架構設計 (Core Architecture in Code)

本專案展示了從 Linux 核心驅動到多語言 ROS 2 微服務的「全端邊緣防禦」拓樸結構，對應的關鍵程式碼如下：

### 🛡️ [OT 層] 混沌生成器 (Kernel Space)
* **位置：** `ot_defense_layer/src/mock_elc_core.c`
* **機制：** 作為 SIL 測試的核心，此 Linux Kernel Module 模擬了極度惡劣的物理環境。它會透過亂數高頻率地產生硬體中斷（如實體火警）、環境異常（粉塵超標）等物理雜訊，藉此對上層 IT 大腦進行極限壓力測試。

### 📜 [實體邊界] 24 Bytes ICD 合約
* **位置：** `ot_defense_layer/include/v5_ioctl_contract.h`
* **機制：** IT 與 OT 的唯一溝通橋樑。透過 `#pragma pack(push, 1)` 強制 1 Byte 記憶體對齊，確保 O(1) 的原子性狀態交換。底層所有的物理雜訊，在越過此介面時皆被收斂為純淨的語意狀態（如 `WARNING`, `EMERGENCY`）。

### 🧠 [IT 層] 核心橋接大腦 (User Space - C++)
* **位置：** `it_edge_layer/ros2_ws/src/v5_core_bridge/src/bridge_node.cpp`
* **機制：** 追求極致效能的防禦核心。透過 `ioctl` 讀取 24 Bytes 快照，並在無塵室沙盒中運行 `V5SafetyFSM` 狀態機，決定全域的降級與保命決策。完全實現「基礎設施無關性 (Infrastructure Agnostic)」。

### 🌐 [IT 層] 跨語言微服務 (User Space - Python)
* **位置：** `m3_access_control` 至 `m7_cloud_forwarder`
* **機制：** 利用 ROS 2 的多語言特性 (Polyglot Microservices)，將門禁控制、環境監控、本地顯示與雲端轉發等商業邏輯，以 Python 節點群實作。所有子系統皆被動訂閱由 C++ 大腦發出的 `/v5/safety_state` 話題進行非同步協作，達成組織與功能的絕對解耦。

---

## 3. 快速啟動 (Quick Start)

無需外接任何實體感測器，透過以下步驟即可在本機端啟動 V5 MCS 架構與 SIL 混沌模擬：

### Step 1: 編譯並載入 OT 層混沌驅動程式
```bash
# 進入 OT 驅動目錄
cd ot_defense_layer

# 編譯 Kernel Module
make

# 載入核心模組 (將在背景開始注入混沌亂數)
sudo insmod v5_elc.ko
```
(提示：可透過 `dmesg -w` 觀察 Kernel Module 產生的底層物理狀態日誌)

### Step 2: 編譯 IT 層 ROS 2 工作區
```bash
# 進入 ROS 2 工作區
cd ../it_edge_layer/ros2_ws

# 使用 colcon 進行全專案編譯
colcon build
```

### Step 3: 一鍵啟動系統與大腦
```bash
# 載入環境變數
source install/setup.bash

# 啟動核心大腦與所有微服務節點
ros2 launch v5_bringup v5_system.launch.py
```
 啟動後，你將在終端機中看到 C++ 橋接節點如何優雅地擋下 `v5_elc.ko` 注入的火警與異常，並協調 Python 微服務進行相對應的降級與警告處置。

---

## 4. 系統邊界與演進軌跡 (Evolution Roadmap)
* **雲端信任擴展：** 目前架構專注於邊緣節點內部的邏輯防禦。得益於 IT/OT 的絕對解耦，未來在 `m7_cloud_forwarder` 引入 Zero Trust (零信任架構) 時，底層的 24 Bytes 驅動與 C++ 防禦狀態機將能達到零修改 (Zero-Code-Change) 的無縫銜接。
* **從 SIL 邁向 HIL (Hardware-In-The-Loop)：** 未來只需將 `mock_elc_core.c` 替換為真實具備 GPIO/I2C 讀取能力的 BSP 驅動，上層龐大的 ROS 2 邏輯即可直接落地於工業現場，完成從模擬到實體的跨越。
