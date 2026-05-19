import rclpy
from rclpy.node import Node
import struct
import queue
import threading
import time
import os

import firebase_admin
from firebase_admin import credentials
from firebase_admin import db

from v5_interfaces.msg import SafetyState

class M7ForwarderNode(Node):
    def __init__(self):
        super().__init__('m7_traffic_shaper_node')
        
        # [邊緣開關] 宣告 ROS 2 動態參數，預設為 True (立刻上傳)
        self.declare_parameter('enable_cloud_sync', True)
        
        # 初始化 Firebase
        key_path = os.path.expanduser('~/mcs-edge-controller/secrets/mcs-edge-controller-firebase-adminsdk-fbsvc-333d192f3f.json')
        try:
            cred = credentials.Certificate(key_path)
            firebase_admin.initialize_app(cred, {
                'databaseURL': 'https://mcs-edge-controller-default-rtdb.asia-southeast1.firebasedatabase.app/'
            })
            self.db_ref = db.reference('v5_edge_system/current_state')
            self.get_logger().info('☁️ Firebase Admin SDK 成功連線至目標實例！')
        except Exception as e:
            self.get_logger().error(f'🛑 Firebase 初始化失敗: {e}')
            raise e

        # 優先權佇列維持最大 100 筆緩存
        self.tx_queue = queue.PriorityQueue(maxsize=100)
        self.sub = self.create_subscription(
            SafetyState, 'safety/semantic_state', self.process_incoming_state, 10)
            
        self.tx_thread = threading.Thread(target=self.transmit_worker, daemon=True)
        self.tx_thread.start()
        
        self.get_logger().info('🛡️ M7 流量塑形閘道器已就緒。可透過參數控制傳輸開關。')

    def process_incoming_state(self, msg):
        try:
            prio = 0 if msg.system_state == SafetyState.STATE_EMERGENCY else 1
            
            noise_raw = int(msg.noise_db * 10)
            pm25_raw = int(msg.pm25 * 10)
            payload = struct.pack('<BHH', msg.system_state, noise_raw, pm25_raw)
            
            try:
                self.tx_queue.put((prio, time.time(), payload), block=False)
            except queue.Full:
                if prio == 0:  
                    self.tx_queue.get_nowait()
                    self.tx_queue.put((prio, time.time(), payload))
                    self.get_logger().warn('⚠️ 本地佇列溢位！丟棄低關鍵性遙測以保護緊急事件。')

        except Exception as e:
            self.get_logger().error(f"數據處理失敗: {e}")

    def transmit_worker(self):
        while rclpy.ok():
            sync_enabled = self.get_parameter('enable_cloud_sync').get_parameter_value().bool_value
            
            if not sync_enabled:
                time.sleep(0.2)
                continue
                
            try:
                prio, t_stamp, payload = self.tx_queue.get(timeout=0.5)
                hex_data = payload.hex().upper()
                
                try:
                    self.db_ref.set({
                        'timestamp': t_stamp,
                        'priority': prio,
                        'payload_hex': hex_data,
                        'gateway_local_time': time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())
                    })
                    self.get_logger().info(f"📤 [雲端同步成功] Prio:{prio} | Payload: 0x{hex_data}")
                except Exception as e:
                    self.get_logger().error(f"☁️ Firebase 寫入失敗 (網路異常): {e}")
                    self.tx_queue.put((prio, t_stamp, payload))
                    time.sleep(1.0)
                
                time.sleep(2.0)
                self.tx_queue.task_done()
            except queue.Empty:
                continue

# 🌟 剛剛遺失的靈魂區塊就在這裡：
def main(args=None):
    rclpy.init(args=args)
    node = M7ForwarderNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()