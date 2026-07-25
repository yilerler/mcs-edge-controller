# 專案架構字典 (Architecture Glossary)

**Repository:** `mcs-edge-controller-5.2.4`

本字典作為專案的「架構正典（Architecture Canon）」，確保所有開發、設計與文件溝通皆具備唯一性，沒有領域歧義。將 Term（架構語彙）、Repository Evidence（對應的實體目錄證據）與 Definition（精確定義）嚴格綁定。

## 架構語彙對照表

| Term (架構語彙) | Repository Evidence (實體目錄證據) | Definition (精確定義) |
| :--- | :--- | :--- |
| **OT Defense Layer** | `ot_defense_layer/` (含 `v5_ioctl_contract.h`) | 確保 OT 決定性執行語意不受 IT 干擾的底層隔離區，僅透過嚴格的 IOCTL 合約向外暴露操作。 |
| **IT Edge Layer** | `it_edge_layer/` | 承載非決定性運算、ROS2 通訊中介軟體及業務邏輯的運行環境。 |
| **Core** | `it_edge_layer/ros2_ws/src/core/` | 系統 Runtime 狀態樞紐與介面定義器，包含 `OTState`、`SafetyState` 等標準訊息結構，絕不涉入具體業務邏輯。 |
| **Gateway** | `it_edge_layer/ros2_ws/src/gateways/` | 專職邊界協定轉換（Protocol Translation）的適配器，分為對內硬體合約的 `ot_gateway` 與對外通訊的 `it_gateway`。 |
| **Service** | `it_edge_layer/ros2_ws/src/services/` | 純粹的應用業務邏輯實體（如 `m3_access_control`, `m4_environment_monitor` 等），依賴 Core 提供的狀態進行運算，無直接硬體控制權。 |
| **Contract** | `v5_ioctl_contract.h` | 跨越 OT 與 IT 邊界時，唯一合法的系統呼叫資料結構與定址定義。 |