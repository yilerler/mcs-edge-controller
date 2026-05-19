# 系統狀態常數
uint8 STATE_NORMAL = 0
uint8 STATE_WARNING = 1
uint8 STATE_DEGRADED = 2
uint8 STATE_EMERGENCY = 3

# 🌟 [新增] 大門狀態常數
uint8 DOOR_LOCKED = 0
uint8 DOOR_PENDING = 1
uint8 DOOR_GRANTED = 2
uint8 DOOR_FORCE_RELEASED = 3

# 實際傳遞的資料
uint8 system_state
uint8 door_state       # 🌟 [新增] 大門狀態
float32 noise_db
float32 pm25
float32 fence_distance # 🌟 [新增] 電子圍籬距離