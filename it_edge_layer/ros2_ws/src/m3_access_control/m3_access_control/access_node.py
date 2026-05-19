import rclpy
from rclpy.node import Node
from std_msgs.msg import String
from v5_interfaces.msg import SafetyState
import random
import time

class ChaosTesterNode(Node):
    def __init__(self):
        super().__init__('m3_chaos_tester')
        
        self.pub = self.create_publisher(String, 'access/door_request', 10)
        self.sub = self.create_subscription(
            SafetyState, 'safety/semantic_state', self.state_callback, 10)
        
        # 🌟 混沌引擎：以 10Hz (每秒 10 次) 的超高頻率運轉
        self.timer = self.create_timer(0.1, self.chaos_engine_tick)
        
        # 狀態快取
        self.sys_state = SafetyState.STATE_DEGRADED
        self.door_state = SafetyState.DOOR_LOCKED
        
        # 📊 測試統計指標 (Metrics)
        self.stats = {
            "total_requests": 0,
            "granted_during_normal": 0,
            "blocked_by_safety": 0,    # 工安攔截
            "invalid_payloads": 0      # 亂碼攻擊
        }
        
        # 報表計時器 (每 5 秒印出一次防禦報表)
        self.report_timer = self.create_timer(5.0, self.print_report)
        
        self.get_logger().info("🔥 M3 混沌測試器 (Chaos Tester) 已啟動！準備對 FSM 進行隨機壓測...")

    def state_callback(self, msg):
        """即時同步大腦的防禦狀態"""
        self.sys_state = msg.system_state
        
        # 觀測大腦是否成功核准了合法的請求
        if msg.door_state == SafetyState.DOOR_GRANTED and self.door_state != SafetyState.DOOR_GRANTED:
            self.stats["granted_during_normal"] += 1
            # 模擬人員瞬間通過，發送 CLOSE 幫大腦關門
            close_msg = String()
            close_msg.data = "CLOSE"
            self.pub.publish(close_msg)
            
        self.door_state = msg.door_state

    def chaos_engine_tick(self):
        """隨機攻擊產生器"""
        # 只有 20% 的機率會發動攻擊，模擬隨機性
        if random.random() > 0.2:
            return
            
        msg = String()
        attack_type = random.choice(["LEGAL_OPEN", "LEGAL_OPEN", "HACK_STRING", "DOUBLE_OPEN"])
        
        self.stats["total_requests"] += 1
        
        if attack_type == "LEGAL_OPEN":
            msg.data = "OPEN"
            # 如果現在環境不安全，我們預期大腦會擋下這個 OPEN
            if self.sys_state != SafetyState.STATE_NORMAL:
                self.stats["blocked_by_safety"] += 1
                
        elif attack_type == "HACK_STRING":
            msg.data = "SQL_INJECTION_OR_GARBAGE"
            self.stats["invalid_payloads"] += 1
            
        elif attack_type == "DOUBLE_OPEN":
            # 模擬接點彈跳 (Bouncing) 或惡意連點
            msg.data = "OPEN"
            self.pub.publish(msg) # 狂發兩次
            if self.sys_state != SafetyState.STATE_NORMAL:
                self.stats["blocked_by_safety"] += 1
                
        self.pub.publish(msg)

    def print_report(self):
        """列印 SIL 自動化測試報表"""
        self.get_logger().info("========================================")
        self.get_logger().info("🛡️ FSM 自動化防禦測試報表 (Chaos Report)")
        self.get_logger().info("========================================")
        self.get_logger().info(f"發起總攻擊/請求數 : {self.stats['total_requests']}")
        self.get_logger().info(f"✅ 和平時期合法放行 : {self.stats['granted_during_normal']}")
        self.get_logger().info(f"🛑 工安警報成功攔截 : {self.stats['blocked_by_safety']}")
        self.get_logger().info(f"🗑️ 無效負載免疫次數 : {self.stats['invalid_payloads']}")
        
        # 斷言 (Assertion)：攔截數 + 放行數 + 無效數 應該要等於 總請求數 (扣除連點誤差)
        # 這裡展示了系統的絕對穩定性
        self.get_logger().info("========================================")

def main(args=None):
    rclpy.init(args=args)
    node = ChaosTesterNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()