import rclpy
from rclpy.node import Node
import random

# 🌟 核心升級：匯入我們自己定義的強型別合約
from v5_interfaces.msg import SafetyState

class AirQualitySensorNode(Node):
    def __init__(self):
        super().__init__('m5_aq_node')
        
        # 1. 建立強型別發布者 (Publisher)
        # 宣告我們只會在 'environment/air_quality' 這個通道上，發布 SafetyState 格式的資料
        self.publisher_ = self.create_publisher(SafetyState, 'environment/air_quality', 10)
        
        # 2. 建立硬體輪詢計時器 (Timer)
        # 0.1 秒觸發一次 (10Hz)，這就是這條神經的心跳頻率
        self.timer = self.create_timer(0.1, self.timer_callback)
        
        self.get_logger().info('🌫️ M5 空品監測神經 (SIL 強型別版) 已上線')

    def timer_callback(self):
        """這是每 0.1 秒會被喚醒一次的核心迴圈"""
        
        # 3. 實體化合約物件
        msg = SafetyState()
        
        # 4. 物理訊號數位化 (此處以 random 模擬讀取真實的 PM2.5 感測器，如 ZE25-O2 等硬體介面)
        msg.pm25 = float(random.uniform(10.0, 200.0))
        
        # 5. 關注點分離 (Separation of Concerns)
        # 因為這份合約包含了 noise_db，但 M5 是嗅覺神經聽不到聲音，所以將其鎖死為 0.0
        msg.noise_db = 0.0 
        
        # 6. 無狀態邊界設計 (Stateless Boundary)
        # 【重要】即使 PM2.5 飆高到 200，M5 也絕對不宣告 WARNING。
        # 它的職責只有「如實呈報」，系統狀態永遠填寫 NORMAL，生殺大權交給 C++ 大腦。
        msg.system_state = SafetyState.STATE_NORMAL 
        
        # 7. 射出資料 (DDS 序列化)
        # ROS 2 底層會自動把這個 Python 物件，壓縮成最高效的 C-Struct 二進制封包送給大腦
        self.publisher_.publish(msg)

def main(args=None):
    rclpy.init(args=args)
    node = AirQualitySensorNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()