import rclpy
from rclpy.node import Node
import queue
import threading
import time
import os
import random # 用於模擬雲端隨機下發的參數

import firebase_admin
from firebase_admin import credentials
from firebase_admin import db

# 🌟 [新增] 引入北向的兩個關鍵合約
from v5_interfaces.msg import SafetyState
from v5_interfaces.msg import CloudIntent

class M7ForwarderNode(Node):
    def __init__(self):
        super().__init__('m7_traffic_shaper_node')
        
        # [邊緣開關] 宣告 ROS 2 動態參數
        self.declare_parameter('enable_cloud_sync', True)
        
        # ==========================================
        # 1. 雲端基礎設施初始化 (維持您的 Firebase 原樣)
        # ==========================================
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

        # ==========================================
        # 2. 上行 (Uplink) 通道：邊緣 ➔ 雲端
        # ==========================================
        self.tx_queue = queue.PriorityQueue(maxsize=100)
        self.sub = self.create_subscription(
            SafetyState, 'safety/semantic_state', self.process_incoming_state, 10)
            
        self.tx_thread = threading.Thread(target=self.transmit_worker, daemon=True)
        self.tx_thread.start()

        # ==========================================
        # 3. 🌟 下行 (Downlink) 通道：雲端 ➔ 邊緣大腦
        # ==========================================
        # 建立北向意圖插座
        self.intent_pub = self.create_publisher(CloudIntent, '/v5/cloud_intent', 10)
        # 模擬雲端配置下發 (每 15 秒模擬一次後台客服修改參數)
        self.mock_cloud_timer = self.create_timer(15.0, self.simulate_cloud_downlink)
        
        self.get_logger().info('🛡️ M7 雙向閘道器已就緒 (具備流量塑形與意圖下行模擬)。')

    # ---------------------------------------------------------
    # [上行] 處理來自 C++ 大腦的語意狀態
    # ---------------------------------------------------------
    def process_incoming_state(self, msg: SafetyState):
        try:
            # 優先權判定 (緊急狀態為最高優先級 0)
            prio = 0 if msg.system_state == SafetyState.STATE_EMERGENCY else 1
            
            # 🌟 重構：廢除 struct.pack hex，改用高可讀性的 JSON 字典
            # 充分利用 v5.2.3 新增的溯源欄位
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

    # ---------------------------------------------------------
    # [上行] 上傳至 Firebase 的執行緒
    # ---------------------------------------------------------
    def transmit_worker(self):
        while rclpy.ok():
            sync_enabled = self.get_parameter('enable_cloud_sync').get_parameter_value().bool_value
            
            if not sync_enabled:
                time.sleep(0.2)
                continue
                
            try:
                prio, t_stamp, payload_dict = self.tx_queue.get(timeout=0.5)
                
                try:
                    # 🌟 重構：直接將 Python Dictionary 寫入 Firebase
                    self.db_ref.set({
                        'timestamp': t_stamp,
                        'priority': prio,
                        'data': payload_dict,
                        'gateway_local_time': time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())
                    })
                    # 降低 Log 頻率或只顯示重要資訊，避免終端機洗版
                    if prio == 0:
                        self.get_logger().info(f"🚨 [緊急上傳成功] 狀態: {payload_dict['system_state']}")
                except Exception as e:
                    self.get_logger().error(f"☁️ Firebase 寫入失敗 (網路異常): {e}")
                    self.tx_queue.put((prio, t_stamp, payload_dict))
                    time.sleep(1.0)
                
                time.sleep(1.0) # 控制上傳頻率 (原 2.0s，微調為 1.0s 讓遙測更順)
                self.tx_queue.task_done()
            except queue.Empty:
                continue

    # ---------------------------------------------------------
    # 🌟 [下行] 模擬接收雲端意圖
    # ---------------------------------------------------------
    def simulate_cloud_downlink(self):
        # 假設這是從 Firebase Listener 收到的最新設定 JSON
        mock_threshold = random.choice([80.0, 100.0, 150.0, 999.0])
        mock_hash = f"v5.2.3_hash_{random.randint(100,999)}"
        
        self.get_logger().warn(f"🔽 [意圖下行] 收到雲端配置更新要求！目標 PM2.5 閾值: {mock_threshold}, Hash: {mock_hash}")

        # 打包成 ROS 2 內部合約，準備餵給 C++ 大腦
        intent_msg = CloudIntent()
        intent_msg.header.stamp = self.get_clock().now().to_msg()
        intent_msg.intent_version_hash = mock_hash
        intent_msg.desired_pm25_threshold = mock_threshold
        intent_msg.desired_noise_threshold_db = 90.0
        intent_msg.force_drill_mode = False
        
        # 發布到 ROS 2 網路
        self.intent_pub.publish(intent_msg)

def main(args=None):
    rclpy.init(args=args)
    node = M7ForwarderNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        if rclpy.ok(): # 確保還活著才去關閉它
            rclpy.shutdown()

if __name__ == '__main__':
    main()