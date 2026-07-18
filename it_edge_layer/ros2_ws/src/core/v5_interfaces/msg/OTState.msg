# 嚴格對應底層 v5_ioctl_contract_t 的物理狀態欄位
std_msgs/Header header

# 門禁與狀態
uint8 rfid_status
uint8 door_lock_state

# 環境感測器 (MQ-3, MQ-135, ZE25-O2)
uint16 voc_ppm
uint16 co_ppm
uint16 o2_concentration

# 噪音與人員健康
uint8 noise_db
uint16 heart_rate_bpm

# 底層硬體錯誤旗標 (用來觸發 FSM 降級)
uint32 hardware_error_flags