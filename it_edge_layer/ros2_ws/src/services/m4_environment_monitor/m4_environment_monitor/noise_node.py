import rclpy
from rclpy.node import Node
import random
# 🌟 [SIL 升級] 引入強型別
from v5_interfaces.msg import SafetyState

class NoiseSensorNode(Node):
    def __init__(self):
        super().__init__('m4_noise_node')
        # 🌟 修改發布型別
        self.publisher_ = self.create_publisher(SafetyState, 'environment/noise', 10)
        self.timer = self.create_timer(0.1, self.timer_callback) # 10Hz

    def timer_callback(self):
        # 🌟 直接建立物件並填值
        msg = SafetyState()
        msg.noise_db = float(random.uniform(50.0, 95.0))
        msg.pm25 = 0.0 # M4 不負責 PM2.5，填 0
        msg.system_state = SafetyState.STATE_NORMAL # 這是感測器，狀態由大腦決定
        
        self.publisher_.publish(msg)

def main(args=None):
    rclpy.init(args=args)
    node = NoiseSensorNode()
    
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()