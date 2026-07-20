import rclpy
from rclpy.node import Node
import curses
import locale  # 🌟 解決中文字元計算錯誤的關鍵

from v5_interfaces.msg import SafetyState
from v5_interfaces.msg import OTState

class M6DisplayNode(Node):
    def __init__(self):
        super().__init__('m6_display_node')
        
        self.semantic_data = SafetyState()
        self.ot_data = OTState()
        
        self.create_subscription(OTState, '/v5/ot_state', self.ot_callback, 10)
        self.create_subscription(SafetyState, 'safety/semantic_state', self.semantic_callback, 10)

    def ot_callback(self, msg):
        self.ot_data = msg

    def semantic_callback(self, msg):
        self.semantic_data = msg

# =========================================================
# 🎨 終端機 UI 繪製邏輯 (Curses)
# =========================================================
def draw_tui(stdscr, node):
    curses.curs_set(0)
    stdscr.nodelay(True)
    
    curses.start_color()
    curses.init_pair(1, curses.COLOR_GREEN, curses.COLOR_BLACK)
    curses.init_pair(2, curses.COLOR_YELLOW, curses.COLOR_BLACK)
    curses.init_pair(3, curses.COLOR_RED, curses.COLOR_BLACK)
    curses.init_pair(4, curses.COLOR_CYAN, curses.COLOR_BLACK)

    while rclpy.ok():
        rclpy.spin_once(node, timeout_sec=0.05)
        stdscr.clear()
        
        # 🌟 取得當前終端機大小 (y, x)
        max_y, max_x = stdscr.getmaxyx()

        sys_idx = min(node.semantic_data.system_state, 3)
        door_idx = min(node.semantic_data.door_state, 3)
        
        sys_state_str = ["NORMAL (常態營運)", "WARNING (局部警報)", "DEGRADED (系統降級)", "EMERGENCY (全域災難)"]
        door_state_str = ["LOCKED (上鎖)", "PENDING (審核中)", "GRANTED (准許通行)", "FORCE_RELEASED (強制釋放)"]
        
        sys_color = curses.color_pair(1) if sys_idx == 0 else (curses.color_pair(3) if sys_idx == 3 else curses.color_pair(2))
        door_color = curses.color_pair(1) if door_idx == 2 else (curses.color_pair(2) if door_idx == 1 else curses.color_pair(3))

        fence_str = "BRAKING (有人闖入)" if node.ot_data.fence_status == 1 else "CLEAR (淨空)"
        fence_color = curses.color_pair(3) if node.ot_data.fence_status == 1 else curses.color_pair(1)

        try:
            # 🌟 Curses 最佳實踐：將所有繪圖邏輯包進 try-except
            # 當終端機被縮太小，字元畫出邊界時，靜默忽略錯誤，而不是崩潰
            row = 1
            stdscr.addstr(row, 2, "======================================================", curses.A_BOLD)
            row += 1
            stdscr.addstr(row, 2, " 🛡️ V5.2.4 工地戰情板 (Hexagonal Aggregator)", curses.A_BOLD)
            row += 1
            stdscr.addstr(row, 2, "======================================================", curses.A_BOLD)
            row += 2

            stdscr.addstr(row, 2, "🧠 [大腦裁決 - Semantic State]", curses.A_BOLD)
            row += 1
            stdscr.addstr(row, 4, f"系統防禦等級: ")
            stdscr.addstr(sys_state_str[sys_idx], sys_color | curses.A_BOLD)
            row += 1
            stdscr.addstr(row, 4, f"門禁管制狀態: ")
            stdscr.addstr(door_state_str[door_idx], door_color | curses.A_BOLD)
            row += 1
            stdscr.addstr(row, 4, f"當前閾值版本: {node.semantic_data.current_config_version}")
            row += 2

            stdscr.addstr(row, 2, "☁️ [邊緣環境 - IT Microservices]", curses.A_BOLD)
            row += 1
            stdscr.addstr(row, 4, f"空氣品質 (PM2.5): {node.semantic_data.pm25:.1f} ug/m3")
            row += 1
            stdscr.addstr(row, 4, f"環境噪音 (Noise): {node.semantic_data.noise_db:.1f} dB")
            row += 2

            stdscr.addstr(row, 2, "⚙️ [底層防線 - OT Hardware]", curses.A_BOLD)
            row += 1
            stdscr.addstr(row, 4, f"電子圍籬狀態: ")
            stdscr.addstr(f"{fence_str}", fence_color | curses.A_BOLD)
            stdscr.addstr(f" (距離: {node.ot_data.fence_distance} mm)", curses.color_pair(4))
            row += 1
            stdscr.addstr(row, 4, f"火警特徵數值: {node.ot_data.fire_heat_value}", curses.color_pair(4))
            row += 1
            stdscr.addstr(row, 4, f"硬體心跳 (HB): {node.ot_data.ot_heartbeat_ms} ms", curses.color_pair(4))
            
            row += 3
            if row < max_y:
                stdscr.addstr(row, 2, "(按 'q' 鍵退出戰情板)", curses.A_DIM)

        except curses.error:
            # 視窗太小被切斷時，不做任何事，保持程式存活
            pass

        stdscr.refresh()

        try:
            key = stdscr.getkey()
            if key == 'q':
                break
        except Exception:
            pass

def main(args=None):
    # 🌟 在啟動 curses 之前，強制設定語系以支援 UTF-8 中文
    locale.setlocale(locale.LC_ALL, '')
    
    rclpy.init(args=args)
    node = M6DisplayNode()
    
    try:
        curses.wrapper(draw_tui, node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()