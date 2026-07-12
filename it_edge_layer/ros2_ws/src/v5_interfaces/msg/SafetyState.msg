std_msgs/Header header

# 系統狀態常數
uint8 STATE_NORMAL = 0
uint8 STATE_WARNING = 1
uint8 STATE_DEGRADED = 2
uint8 STATE_EMERGENCY = 3

# 大門狀態常數
uint8 DOOR_LOCKED = 0
uint8 DOOR_PENDING = 1
uint8 DOOR_GRANTED = 2
uint8 DOOR_FORCE_RELEASED = 3

# 大腦決策語意
uint8 system_state
uint8 door_state

# 供雲端儀表板顯示的精煉遙測
float32 noise_db
float32 pm25
float32 fence_distance

# 🌟 [v5.2.3 新增] 數位孿生與對帳溯源
string current_config_version
string override_source