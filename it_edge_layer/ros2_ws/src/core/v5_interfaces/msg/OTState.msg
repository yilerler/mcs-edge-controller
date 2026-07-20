# V5 OT 實體防禦狀態報告 (OT Gateway -> Bridge Node)
# ⚠️ 嚴格對齊 Kernel: v5_ioctl_contract_t (12 Bytes 完美瘦身版) ⚠️

std_msgs/Header header

# 領域 1：全域系統防禦
uint8 ot_system_level      # 對應 V5_STATE_* 

# 領域 2：M2 智慧電子圍籬
uint8 fence_status         # 對應 V5_FENCE_* 
uint32 fence_distance      # 實體測距數值 (mm)

# 領域 3：火災與環境硬體防禦
uint16 fire_heat_value     # 實體火警溫度/特徵值

# 領域 4：硬體心跳
uint32 ot_heartbeat_ms     # 來自微控制器的開機微秒/毫秒數