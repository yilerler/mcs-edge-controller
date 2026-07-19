"""
IT Gateway Node (北向閘道器).

本模組作為 V5 工地安全管理系統的北向邊界，負責 ROS 2 內部網域與外部 IT 網路 (Firebase/Cloud) 的隔離與通訊。
嚴格遵守 Hexagonal Architecture 原則：僅進行通訊協定轉換與資料型別驗證 (Type Checking)，絕對不包含任何安全領域決策邏輯。
"""

import rclpy
from rclpy.node import Node
import queue
import threading
import time
import os
import random

import firebase_admin
from firebase_admin import credentials
from firebase_admin import db

from v5_interfaces.msg import SafetyState
from v5_interfaces.msg import CloudIntent


class ITGatewayNode(Node):
    """
    執行雙向協定翻譯與網路錯誤隔離的邊緣閘道器節點。

    特徵：
    1. 上行 (Uplink)：具備 Priority Queue 進行流量塑形，確保緊急狀態不被遙測數據阻塞。
    2. 下行 (Downlink)：攔截並過濾非法的雲端資料型別，保護底層 C++ 領域核心免受格式錯誤攻擊。
    """

    def __init__(self):
        """初始化 IT Gateway，建立雲端連線與非同步上傳執行緒。"""
        super().__init__('v5_it_gateway_node')
        
        self.declare_parameter('enable_cloud_sync', True)
        
        # 1. 雲端基礎設施初始化
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

        # 2. 上行 (Uplink) 通道：邊緣 ➔ 雲端
        self.tx_queue = queue.PriorityQueue(maxsize=100)
        self.sub = self.create_subscription(
            SafetyState, 'safety/semantic_state', self.process_incoming_state, 10)
            
        self.tx_thread = threading.Thread(target=self.transmit_worker, daemon=True)
        self.tx_thread.start()

        # 3. 下行 (Downlink) 通道：雲端 ➔ 邊緣大腦
        self.intent_pub = self.create_publisher(CloudIntent, '/v5/cloud_intent', 10)
        
        # TODO [v5.4 雲地閉環]: IT_GATEWAY - 移除定時模擬器，改接 Firebase 異動事件監聽。
        self.mock_cloud_timer = self.create_timer(15.0, self.simulate_cloud_downlink)
        
        self.get_logger().info('🛡️ IT 雙向閘道器已就緒 (守護北向防禦邊界)。')

    def process_incoming_state(self, msg: SafetyState):
        """
        將 ROS 2 語意狀態轉譯為 JSON 字典，並推入優先權佇列。

        Args:
            msg (SafetyState): 來自 Bridge Node 的領域狀態發布。
        """
        try:
            prio = 0 if msg.system_state == SafetyState.STATE_EMERGENCY else 1
            
            semantic_payload = {
                "system_state": msg.system_state,
                "door_state": msg.door_state,
                "telemetry": {
                    "noise_db": round(msg.noise_db, 1),
                    "pm25": round(msg.pm25, 1)
                },
                "sync_meta": {
                    "running_config": msg.current_config_version,
                    "override": msg.override_source
                }
            }
            
            try:
                self.tx_queue.put((prio, time.time(), semantic_payload), block=False)
            except queue.Full:
                if prio == 0:  
                    self.tx_queue.get_nowait()
                    self.tx_queue.put((prio, time.time(), semantic_payload))
                    self.get_logger().warn('⚠️ 本地佇列溢位！丟棄低關鍵性遙測以保護緊急事件。')

        except Exception as e:
            self.get_logger().error(f"上行數據處理失敗: {e}")

    def transmit_worker(self):
        """
        負責將佇列資料非同步上傳至 Firebase 的獨立執行緒。

        此函式刻意隔離於主 Spin 迴圈外，防止網路延遲造成系統假死 (Deadlock)。
        """
        while rclpy.ok():
            sync_enabled = self.get_parameter('enable_cloud_sync').get_parameter_value().bool_value
            
            if not sync_enabled:
                time.sleep(0.2)
                continue
                
            try:
                prio, t_stamp, payload_dict = self.tx_queue.get(timeout=0.5)
                
                try:
                    self.db_ref.set({
                        'timestamp': t_stamp,
                        'priority': prio,
                        'data': payload_dict,
                        'gateway_local_time': time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())
                    })
                    if prio == 0:
                        self.get_logger().info(f"🚨 [緊急上傳成功] 狀態: {payload_dict['system_state']}")
                except Exception as e:
                    self.get_logger().error(f"☁️ Firebase 寫入失敗 (網路異常): {e}")
                    self.tx_queue.put((prio, t_stamp, payload_dict))
                    time.sleep(1.0)
                
                time.sleep(1.0)
                self.tx_queue.task_done()
            except queue.Empty:
                continue

    def simulate_cloud_downlink(self):
        """
        模擬從雲端接收意圖，進行嚴格型別檢查後轉譯為 ROS 2 內部合約。

        此函式展現了 IT Gateway 的核心防禦價值：絕不讓不合法的資料型別 (如字串混入浮點數) 
        污染底層 C++ 的純淨記憶體空間。
        """
        # 假設這是從 Firebase 拿到的 Dictionary (可能含有惡意或錯誤型別)
        mock_raw_data = {
            "pm25_threshold": random.choice([80.0, 100.0, 150.0, 999.0, "ILLEGAL_STRING"]),
            "intent_hash": f"v5.2.3_hash_{random.randint(100,999)}"
        }
        
        # 🛡️ 邊界防禦：型別與格式校驗 (Type Checking)
        if not isinstance(mock_raw_data["pm25_threshold"], (float, int)):
            self.get_logger().error(f"❌ [防禦攔截] 雲端參數型別錯誤！預期 float，收到: {type(mock_raw_data['pm25_threshold'])}")
            return # 直接丟棄，保護大腦
            
        mock_threshold = float(mock_raw_data["pm25_threshold"])
        mock_hash = str(mock_raw_data["intent_hash"])
        
        self.get_logger().warn(f"🔽 [意圖下行] 收到雲端配置更新要求！目標 PM2.5 閾值: {mock_threshold}, Hash: {mock_hash}")

        # 打包成嚴謹的內部介面
        intent_msg = CloudIntent()
        intent_msg.header.stamp = self.get_clock().now().to_msg()
        intent_msg.intent_version_hash = mock_hash
        intent_msg.desired_pm25_threshold = mock_threshold
        intent_msg.desired_noise_threshold_db = 90.0
        intent_msg.force_drill_mode = False
        
        self.intent_pub.publish(intent_msg)


def main(args=None):
    """節點進入點。"""
    rclpy.init(args=args)
    node = ITGatewayNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == '__main__':
    main()