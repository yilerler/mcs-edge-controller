# 01_Architecture_Glossary.md

**Repository:** `mcs-edge-controller-5.3.0`

本字典與目錄作為專案的「架構正典（Architecture Canon）」，確保所有開發、設計與文件溝通皆具備唯一性，消弭領域歧義。本文件將 Term（架構語彙）、Repository Evidence（實體目錄證據）與 Definition（客觀約束定義）嚴格綁定，**並以「約束優先（Constraint-First）」的原則視覺化全域邊界。**

---

## 📖 架構知識庫 (AKB) 導覽目錄

本專案的 AKB 嚴格依照系統的權力階層（由硬到軟、由底層到應用層）進行遞進式定義。欲深入了解系統各層級的防禦機制與設計意圖，請依序參閱以下正典文件：

1. **[01_Architecture_Glossary.md](./01_Architecture_Glossary.md) (本文件)**
   * **定位：** 系統總覽與語意基準。
   * **內容：** 系統全域邊界視圖、權力拓樸圖，以及全域架構語彙對照表。
2. **[02_OT_Defense_Layer.md](./02_OT_Defense_Layer.md)**
   * **定位：** L0 實體防禦層 (Kernel Space)。
   * **內容：** 決定性安全迴圈 (Deterministic Safety Loop) 的實作、實體自治煞車權限，以及核心驅動模組的防禦機制。
3. **[03_ICD_Contract.md](./03_ICD_Contract.md)**
   * **定位：** L0.5 靜態合約界 (The Boundary)。
   * **內容：** 12 Bytes 剛性 ABI 記憶體佈局、編譯期斷言約束，以及跨越 Kernel/User Space 的 `ioctl` 物理屏障設計。
4. **[04_Core_Bridge_FSM.md](./04_Core_Runtime.md)**
   * **定位：** L1 領域大腦 (The SSOT)。
   * **內容：** 純 C++ 領域狀態機 (FSM) 轉移矩陣、邊緣狀態裁量權，以及單一真理來源的維護機制。
5. **[05_Gateways_ACL.md](./05_Gateway.md)**
   * **定位：** L2 防腐層 (Anti-Corruption Layers)。
   * **內容：** 北向閘道的「雲端意圖否決 (Intent Veto) / 型別攔截」與南向閘道的「硬體崩潰優雅降級」實作。
6. **[06_Stateless_Services.md](./06_Service.md)**
   * **定位：** L3 無狀態微服務 (Edge Services)。
   * **內容：** 星型拓樸 (Star Topology) 部署約束、決策權剝離 (State Stripped) 原則，以及零水平耦合的 DDS 匯流排設計。

---

## 一、架構層級全景圖 (Architecture Constraint Diagram)

本圖表不描繪單純的「資料快樂路徑 (Happy Path)」，而是揭示系統的「權力與約束 (Authority & Constraints)」。圖中的防護罩、隔離線與單向箭頭，皆受底層程式碼嚴格支撐。

```text
======================= WAN / CLOUD (External IT) =======================
                                │ (JSON Payload)
                                ▼
+-----------------------------------------------------------------------+
| L2: 北向防腐層 [v5_it_gateway] (Anti-Corruption)                        |
| [ TYPE SHIELD ] (型別攔截)                 [ PRIORITY QUEUE ] (流量塑形) |
+-------------------------------│---------------------------------------+
                                ▼ (CloudIntent)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                        ROS 2 DDS 內部匯流排 (STAR TOPOLOGY)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     │               │               │               │             ▲
+====│====+     +====│====+     +====│====+     +====│====+        │
|L3: M3   |     |L3: M4   |     |L3: M5   |     |L3: M6   |        │
|[CHAOS   |     |[STATE   |     |[STATE   |     |[CRASH   |        │
| ENGINE] |     |STRIPPED]|     |STRIPPED]|     | SHIELD] |        │
+=========+     +=========+     +=========+     +=========+        │
     │               │               │               │             │
     └───────────────┴───────┬───────┴───────────────┘             │
[ ZERO HORIZONTAL COUPLING ] │ (Telemetry / Requests)              │
                             ▼                                     │
           ╔════════════════════════════════════════════════╗      │
           ║ L1: 領域大腦 [v5_core_bridge] (ROS 2 Shell)     ║      │
           ║               [ VETO BARRIER ] (意圖否決)       ║      │
           ║ - - - - - - - - - - - - - - - - - - - - - - - -║      │
           ║ [ Pure C++ FSM ] (Single Source of Truth)      ║──(SafetyState)
           ╚════════════════════════════════════════════════╝       
                             ▲                                      
                             │ (OTState)                            
+----------------------------│------------------------------------------+
| L2: 南向防腐層 [v5_ot_gateway] (Anti-Corruption)                        |
| [ FALLBACK GENERATOR ] (硬體優雅降級保底)                                |
+----------------------------▲--------------------------------------▲---+
                             │ (IOCTL Polling)                      ║
[ WRITE REJECTED ]           ▼       [ READ PERMITTED ]             ║
X════════════════════════════╬══════════════════════════════════════║
 L0.5: 靜態合約 [v5_ioctl_contract.h] (雙向編譯斷言 SSOT / 12 Bytes)   ║
X════════════════════════════╬══════════════════════════════════════║
                             ▼ (Overwritten)                        ║
+-------------------------------------------------------------------╫---+
| L0: 實體防禦層 [ot_defense_layer] (Kernel Space)                    ║   |
| [ MUTEX REGION ] (絕對原子性)   [ AUTONOMOUS SAFETY LOOP ] (實體自治)╝   |
+-----------------------------------------------------------------------+
==================== PHYSICAL SENSORS / ACTUATORS =======================

```

---

## 二、架構語彙對照表 (Ubiquitous Language)

### 1. 系統模組與層級 (System Modules & Levels)

| Term (架構語彙) | Repository Evidence | Definition (客觀約束定義) |
| --- | --- | --- |
| **L0: OT Defense Layer** | `ot_defense_layer/src/` | **[實體防禦區]** 確保 OT 決定性執行語意不受 IT 軟體干擾的底層隔離區。**約束：拒絕任何來自 IT 的狀態寫入，並具備無視 IT 的實體自治煞車能力。** |
| **L0.5: Contract (ICD)** | `v5_ioctl_contract.h` | **[靜態合約界]** 橫跨 Kernel/User Space 的編譯期約束。**約束：無執行期實體，強制鎖定 12 Bytes 記憶體佈局，不允許任何動態大小指標穿越。** |
| **L1: Core Bridge** | `core/v5_core_bridge/` | **[領域大腦]** 系統唯一的全域狀態樞紐與決策引擎。**約束：採用六角形架構保護純 C++ FSM，禁止直接控制硬體，但握有最高級別的狀態裁量主權。** |
| **L2: Gateway (ACL)** | `gateways/` | **[南北防腐層]** 專職吸收外部毒性的適配器。**約束：絕對禁止包含業務決策邏輯。負責攔截動態型別注入（北向）與屏蔽硬體崩潰（南向）。** |
| **L3: Services** | `services/` | **[無狀態孤島]** 邊緣應用的執行個體（如 `m3`, `m4` 等）。**約束：全面剝奪本地決策權（State Stripped），嚴禁微服務之間產生任何水平連線（Zero Horizontal Coupling）。** |

### 2. 核心架構機制 (Core Architecture Mechanisms)

| Term (架構語彙) | Repository Evidence | Definition (客觀約束定義) |
| --- | --- | --- |
| **Anti-Corruption Layer (ACL)** | `it_gateway_node.py`<br>`ot_gateway_node.cpp` | **防腐層。** 架構的邊緣護城河，透過 `[ TYPE SHIELD ]` 與 `[ FALLBACK GENERATOR ]` 將網路不確定性與髒資料徹底阻擋於 DDS 匯流排之外。 |
| **Intent Veto (意圖否決權)** | `bridge_node.cpp` | **邊緣主權展現。** 在 L1 外殼層實施的防護屏障 `[ VETO BARRIER ]`。當實體處於危險狀態時，強制丟棄雲端的下行配置，防止錯誤指令覆蓋本地安全狀態。 |
| **State Stripped (無狀態化)** | `aq_node.py` 等 | **決策權剝離。** L3 感測節點被強制硬編碼寫入 `STATE_NORMAL`。確保節點只能如實呈報數據，消除分散式系統中多節點競爭決策（Split-brain）的風險。 |
| **Star Topology** | ROS 2 Topic 規劃 | **星型拓樸。** L3 節點彼此完全盲目。所有遙測數據必須上傳至 DDS 匯流排並由 L1 統籌，強制建立系統的單一真理來源（SSOT）。 |
| **Memory Atomicity** | `mock_elc_core.c` | **記憶體原子性。** L0 層利用 `[ MUTEX REGION ]` (Spinlock) 鎖定跨環境讀取，徹底免除高頻輪詢下資料破裂 (Torn Read) 與競態條件的可能。 |


