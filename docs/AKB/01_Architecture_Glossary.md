# 01_Architecture_Glossary.md

**Repository:** `mcs-edge-controller-5.2.4`

本字典作為專案的「架構正典（Architecture Canon）」，確保所有開發、設計與文件溝通皆具備唯一性，消弭領域歧義。將 Term（架構語彙）、Repository Evidence（實體目錄證據）與 Definition（客觀工程定義）嚴格綁定。

---

## 一、架構層級全景圖 (Architecture Level Diagram)

本系統依循六角形架構與邊緣主權原則，將軟硬體與網路邊界嚴格劃分為五個架構層級。高層級依賴低層級的資料，但決策權 100% 收斂於 Level 1 的核心大腦。

```text
       [ 外部 WAN / Firebase 雲端 ]                [ 本地終端 / 應用介面 ]
                ^                                        ^
                | (非同步網路 I/O)                         | (UI 渲染/使用者操作)
                v                                        v
+=============================================================================+
| Level 3 (應用層): Stateless Edge Microservices (services/)                   |
|   [M3 門禁與混沌測試]   [M4 噪音感測]   [M5 空氣品質]   [M6 終端戰情板]            |
+=======================================+=====================================+
                ^                       |                       ^
                | (DDS Pub/Sub)         | (DDS Pub/Sub)         | (DDS Pub/Sub)
                v                       v                       v
+=======================================+=====================================+
| Level 2 (防腐層): Anti-Corruption Layer (gateways/)                          |
|   [v5_it_gateway: 雲端型別過濾與緩衝]       [v5_ot_gateway: IOCTL 硬體適配]       |
+=======================================+=====================================+
                ^                       |                       ^
                | (DDS 意圖下放)          | (DDS 語意發布)          | (DDS 實體快照)
                v                       v                       v
+=======================================+=====================================+
| Level 1 (核心層): Edge Semantic Brain (core/)                                |
|   [v5_core_bridge: V5SafetyFSM 領域策略引擎 / 全域看門狗 / 意圖否決裁決所]       |
+=============================================================================+
                                        | (嚴格 12 Bytes 靜態資料結構)
+---------------------------------------+-------------------------------------+
| Level 0.5 (合約層): ABI Contract Boundary (v5_ioctl_contract.h)             |
+---------------------------------------+-------------------------------------+
                                        | (C Struct / IOCTL)
+=============================================================================+
| Level 0 (實體層): Hardware/Kernel Interface (ot_defense_layer/)              |
|   [mock_elc_core.c: Spinlock 實體狀態鎖定 / 硬體中斷處理 / 自主安全降級]         |
+=============================================================================+
                ^
                | (GPIO / UART / 硬體中斷)
                v
      [ 實體世界感測器 / 致動器 (Hardware) ]

```

---

## 二、架構語彙對照表 (Ubiquitous Language)

### 1. 系統模組與層級

| Term (架構語彙) | Repository Evidence | Definition (精確定義) |
| --- | --- | --- |
| **OT Defense Layer** | `ot_defense_layer/` | **[Level 0]** 確保 OT 決定性執行語意不受 IT 干擾的底層隔離區，僅透過嚴格的 IOCTL 合約向外暴露唯讀物理快照。 |
| **Contract (ICD)** | `v5_ioctl_contract.h` | **[Level 0.5]** 跨越 OT 與 IT 邊界時，唯一合法的 C 語言資料結構（精確控制為 12 Bytes），具備編譯期斷言防護。 |
| **Core Bridge** | `core/v5_core_bridge/` | **[Level 1]** 系統 Runtime 狀態樞紐與領域決策引擎，負責執行 FSM 邏輯與發布仲裁結果，保有最終決策主權。 |
| **Gateway (ACL)** | `gateways/` | **[Level 2]** 專職防腐與協定轉換的適配器。對內封裝硬體 API (`ot_gateway`)，對外隔離網路抖動與型別污染 (`it_gateway`)。 |
| **Service (Microservices)** | `services/` | **[Level 3]** 純粹的應用實體（如 `m3`, `m4`, `m5` 等）。依賴 Core 提供的狀態進行運作，不具備決策權與底層硬體直接控制權。 |

### 2. 核心架構概念

| Term (架構語彙) | Repository Evidence | Definition (精確定義) |
| --- | --- | --- |
| **Anti-Corruption Layer (ACL)** | `gateways/v5_it_gateway` | **防腐層。** 將外部系統（如 Firebase）的不穩定性、延遲與髒資料攔截在網域邊界，保護內部核心邏輯不被污染。 |
| **Semantic State** | `SafetyState.msg` | **語意狀態。** 經過 Level 1 (Core FSM) 融合物理特徵與環境變數後，推演出的全域防禦級別（如 `WARNING`, `DEGRADED`）。 |
| **Stateless Boundary** | `aq_node.py` 等 | **無狀態邊界。** 限制 Level 3 感測微服務僅能如實呈報數據，嚴禁包含本地閾值判定與警報決策權的架構約束。 |
| **Intent Veto (邊緣主權)** | `bridge_node.cpp` | **意圖否決權。** 邊緣控制器在判定實體處於危險狀態時，主動拒絕執行雲端 `CloudIntent` 配置指令的防禦機制。 |
| **Hardware Abstraction** | `ot_gateway_node.cpp` | **硬體抽象。** 將 `ioctl()` 系統呼叫封裝，使 ROS 2 網域內的節點無需具備 Kernel 開發知識與權限即可訂閱物理狀態。 |