import rclpy
from rclpy.node import Node
import curses
import time
from v5_interfaces.msg import SafetyState

class DisplayNode(Node):
    def __init__(self):
        super().__init__('m6_display_node')
        self.sub = self.create_subscription(SafetyState, 'safety/semantic_state', self.state_callback, 10)
        
        self.sys_state = SafetyState.STATE_DEGRADED
        self.door_state = SafetyState.DOOR_LOCKED  # 🌟 新增內部變數
        self.noise_db = 0.0
        self.pm25 = 0.0
        self.fence_distance = 0.0  # 🌟 新增內部變數
        self.last_update = time.time()

    def state_callback(self, msg):
        self.sys_state = msg.system_state
        self.door_state = msg.door_state          # 🌟 取值
        self.noise_db = msg.noise_db
        self.pm25 = msg.pm25
        self.fence_distance = msg.fence_distance  # 🌟 取值
        self.last_update = time.time()

def draw_tui(stdscr, node):
    curses.curs_set(0)
    stdscr.nodelay(1)
    curses.start_color()
    curses.init_pair(1, curses.COLOR_GREEN, curses.COLOR_BLACK)
    curses.init_pair(2, curses.COLOR_YELLOW, curses.COLOR_BLACK)
    curses.init_pair(3, curses.COLOR_WHITE, curses.COLOR_RED)
    curses.init_pair(4, curses.COLOR_WHITE, curses.COLOR_BLACK)

    while rclpy.ok():
        rclpy.spin_once(node, timeout_sec=0.1)
        stdscr.clear()
        
        if (time.time() - node.last_update) > 3.0:
            display_str = "OFFLINE (連線中斷)"
            sys_color = curses.color_pair(4)
            door_str = "UNKNOWN"
        else:
            # 系統狀態
            if node.sys_state == SafetyState.STATE_NORMAL:
                display_str = "NORMAL (常態營運)"; sys_color = curses.color_pair(1)
            elif node.sys_state == SafetyState.STATE_WARNING:
                display_str = "WARNING (局部警報)"; sys_color = curses.color_pair(2)
            elif node.sys_state == SafetyState.STATE_DEGRADED:
                display_str = "DEGRADED (系統降級)"; sys_color = curses.color_pair(2)
            elif node.sys_state == SafetyState.STATE_EMERGENCY:
                display_str = "EMERGENCY (全域災難)"; sys_color = curses.color_pair(3) | curses.A_BLINK
            else:
                display_str = "UNKNOWN"; sys_color = curses.color_pair(4)

            # 🌟 大門狀態解析
            if node.door_state == SafetyState.DOOR_LOCKED:
                door_str = "🔒 SECURE_LOCKED"
            elif node.door_state == SafetyState.DOOR_PENDING:
                door_str = "⏳ AUTH_PENDING (審核中)"
            elif node.door_state == SafetyState.DOOR_GRANTED:
                door_str = "🔓 ACCESS_GRANTED"
            elif node.door_state == SafetyState.DOOR_FORCE_RELEASED:
                door_str = "🚨 FORCE_RELEASED (火警強制開啟)"
            else:
                door_str = "UNKNOWN"

        stdscr.border(0)
        stdscr.addstr(1, 2, "=== V5.2 現場安全戰情板 (OT 邏輯完整版) ===", curses.A_BOLD)
        stdscr.addstr(3, 2, f"系統狀態: [ {display_str} ]", sys_color | curses.A_BOLD)
        stdscr.addstr(5, 2, f"大門狀態: {door_str}", curses.A_BOLD)
        
        # 🌟 圍籬距離警示色邏輯
        fence_color = curses.color_pair(1) if node.fence_distance > 100.0 else curses.color_pair(2)
        stdscr.addstr(7, 2, f"圍籬距離: {node.fence_distance:6.1f} cm", fence_color)
        
        stdscr.addstr(8, 2, f"即時噪音: {node.noise_db:6.1f} dB")
        stdscr.addstr(9, 2, f"即時粉塵: {node.pm25:6.1f} µg/m³")
        
        stdscr.addstr(12, 2, "按 'q' 退出", curses.color_pair(4))
        stdscr.refresh()
        
        if stdscr.getch() == ord('q'):
            break

def main(args=None):
    rclpy.init(args=args)
    node = DisplayNode()
    try:
        # 交給 curses 掌管終端機螢幕
        curses.wrapper(draw_tui, node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()