# MCS Edge Controller

MCS Edge Controller 是一個工業級邊緣控制系統。本系統透過合約驅動（Contract-Driven）架構，將「具備決定性（Deterministic）的 OT 安全執行層」與「非決定性（Nondeterministic）的 IT 應用微服務」進行了嚴格的實體與邏輯隔離。



## 🎯 核心特性 (Key Capabilities)

- **絕對的 OT 隔離 (Absolute OT Isolation):** 確保底層的安全防禦迴圈不會被上層 IT 網路的延遲或崩潰所中斷。
- **合約驅動邊界 (Contract-Driven Boundary):** IT 與 OT 之間透過編譯期斷言（Compile-time Asserted）的 12 Bytes ABI 靜態合約進行通訊。
- **語意決策大腦 (Semantic Edge Brain):** 領域狀態機 (FSM) 融合實體感測數據與雲端意圖，作為全系統唯一的真理來源 (SSOT)。
- **無狀態微服務 (Stateless Microservices):** 基於 ROS 2 的星型拓樸 (Star Topology) 部署周邊應用，微服務之間達到零水平耦合 (Zero Horizontal Coupling)。



## 🏗️ 系統拓樸概覽 (Architecture at a Glance)

本拓樸圖不描繪單純的「資料流」，而是揭示系統的**邊界與權力約束**。全系統唯有 L1 具備狀態裁量權，L0 則具備絕對的實體防護主權。

```text
                 [ CLOUD / WAN ]
                       │
                 [ L2: IT ACL ]
                       │
                       ▼
                 ┌─────────┐
    [ L3 ] ─────►│   L1    │◄───── [ L3 ]
 (Stateless)     │  CORE   │    (Stateless)
    [ L3 ] ─────►│ (SSOT)  │◄───── [ L3 ]
                 └────┬────┘
                      │
                 [ L2: OT ACL ]
                      │
               [ L0.5: ABI Contract ]
                      │
               [ WRITE REJECTED ]
                      │
                 [ L0: OT Defense ]
                      │
               [ AUTONOMOUS LOOP ]

```

> 欲深入了解每個層級的嚴格邊界定義與防禦機制，請參閱 **[架構正典 (Architecture Glossary)](./docs/AKB/01_Architecture_Glossary.md)**。



## 🗺️ 文件導航 (Documentation Navigation)

本專案採用「工件中介的架構治理 (Artifact-Mediated Architecture Governance)」。我們將「系統是什麼」、「為什麼這樣設計」以及「如何維持設計」的知識嚴格分離：

📘 **[Architecture Knowledge Base (AKB)](./docs/AKB/)**
* **What is true now?** — 描述當下的架構正典、邊界定義與視覺化約束。


📙 **[Architecture Decision Records (ADR)](./docs/ADR/)**
* **Why is it designed this way?** — 紀錄架構演進的歷史脈絡、技術妥協與預期後果。


⚖️ **[Architecture Governance](./docs/ARCHITECTURE_GOVERNANCE.md)**
* **How do we maintain this system?** — 定義程式碼、架構圖與文件之間如何對帳與除錯的最高治理法則。



## 🚀 快速上手 (Quick Start)

### 系統需求 (Prerequisites)
- **作業系統 (OS)**: Ubuntu 22.04 LTS
- **中介軟體 (Middleware)**: ROS 2 Jazzy (Target / Testing Environment)
- **硬體架構 (Hardware)**: AArch64 / Raspberry Pi 5 (Target Environment)

---

### 建置與部署 (Build & Setup)

本系統嚴格實施 **「合約優先（Contract-First）」** 編譯守則。為降低跨網域（Kernel/User Space）的開發勞苦，同時確保防禦邊界不被破壞，所有實體層部署與 ROS 2 拓樸建置皆已封裝於專案根目錄的全局 `Makefile` 中。

請於**專案根目錄**執行以下指令：

```bash
# 一鍵貫穿物理與邏輯邊界 (將要求 sudo 權限以鎖定 12 Bytes 剛性合約)
make all

```

> 💡 **底層行徑解析 (Under the Hood):**
> 此腳本將自動執行以下防禦性部署：
> 1. **焦土清理：** 徹底抹除 `build/` 與 `install/` 快取，防止型別污染。
> 2. **部署 OT 實體防禦層：** 進入 Kernel Space 編譯並 `insmod mock_elc_core.ko` 驅動，奠定物理防線。
> 3. **建置 IT 邊緣層：** 回到 User Space 優先獨立編譯 `v5_interfaces` 合約，再載入環境變數編譯其餘微服務。
> 
> 

---

### 系統啟動 (Deterministic Wave Launch)

系統導入「由硬到軟、由內而外」的決定性波次啟動 (Wave Launch) 戰略。腳本將在背景以無頭（Headless）守護行程模式依序拉起節點，徹底消弭啟動初期的 Race Condition。

```bash
# 啟動總指揮官（一鍵依序啟動 Wave 0 至 Wave 3）
make launch

```

---

### 觀測與除錯 (Debug & Monitor)

為避免 Python `curses` 函式庫與背景 `ros2 launch` 的輸出重定向產生 TTY 佔用衝突，系統嚴格實施「背景守護與 UI 觀測絕對解耦」原則。

請 **開啟一個全新的終端機分頁（New Tab）**，確保在專案根目錄下獨立運行戰情板：

```bash
# 啟動 M6 本地戰情板 (獨佔當前 TTY 終端機)
make tui

```

---

### 故障排除與手動介入 (Troubleshooting & Manual Fallback)

本系統封裝了極深的底層操作。若您的硬體環境與目標平台不一致，或在執行 `make all` 時遭遇中斷，請**切勿強行重新啟動**。請依序展開以下折疊面板，進行物理與邏輯邊界的狀態盤點。

<details>
<summary><b>1. 檢驗 OT 防禦層狀態 (Kernel Space Check)</b></summary>
<br>

若系統無法鎖定 12 Bytes ABI 合約，通常是因為 LKM 掛載失敗。請手動介入檢查核心狀態：

```bash
# 檢查實體防禦模組是否已成功搶佔核心資源
lsmod | grep mock_elc_core

# 檢視底層防線的系統日誌 (應看見 "Pure Physical Defense Shield initialized")
dmesg | tail -n 20

```

若需手動卸載並重新編譯：

```bash
sudo rmmod mock_elc_core
cd ot_defense_layer/src && make clean && make
sudo insmod mock_elc_core.ko

```

若 `make launch` 無法找到特定節點或訊息型別，通常是舊版快取導致的型別污染。請手動進行焦土重建：

```bash
# 退回 ROS 2 工作區
cd it_edge_layer/ros2_ws

# 執行深度清理
rm -rf build/ install/ log/

# 嚴格遵循合約優先編譯 (先編譯介面，再編譯實作)
colcon build --packages-select v5_interfaces
source install/setup.bash
colcon build --symlink-install

```

若上述物理手段皆無法排除故障，代表系統可能遭遇了未預期的架構性崩潰（例如：狀態機卡死於非法狀態、DDS 網域衝突）。
請立即參閱 **[Architecture Governance (架構治理法典)](./docs/ARCHITECTURE_GOVERNANCE.md)** 中的「異常處理與對帳機制」，透過檢視 SSOT 狀態轉移日誌來釐清問題根源。
</details>


## 📜 授權 (License)

This project is licensed under the **[Apache License 2.0](LICENSE)** - see the full text for details.

Copyright (c) 2024-2026 Joshua Lin. All rights reserved.


