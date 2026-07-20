"""
V5 系統總指揮官 (System Bringup Launch)

本啟動檔落實了嚴格的 IT/OT 六角形架構啟動波次 (Wave Launch)：
- [Wave 0] 物理奠基：啟動 OT Gateway 佔用硬體。
- [Wave 1] 大腦甦醒：等待 OT 啟動後，喚醒 Bridge Node。
- [Wave 2] 服務展開：等待大腦啟動後，並發啟動所有邊緣微服務。
- [Wave 3] 北向放行：等待微服務穩定後，啟動 IT Gateway 連接 Firebase。

同時全面啟用 respawn=True，確保 Day 2 的 Always ON 基礎設施自癒能力。
"""

from launch import LaunchDescription
from launch.actions import RegisterEventHandler, LogInfo, TimerAction
from launch.event_handlers import OnProcessStart
from launch_ros.actions import Node

def generate_launch_description():

    # ========================================================================
    # 節點定義區 (皆設定 respawn=True 以確保 Day 2 自癒能力)
    # ========================================================================
    
    ot_gateway_node = Node(
        package='v5_ot_gateway',
        executable='ot_gateway_node',
        name='ot_gateway_node',
        output='screen',
        respawn=True,
        respawn_delay=2.0
    )

    bridge_node = Node(
        package='v5_core_bridge',
        executable='bridge_node',
        name='bridge_node',
        output='screen',
        respawn=True,
        respawn_delay=2.0
    )

    # 邊緣 IT 業務微服務
    m3_access_node = Node(
        package='m3_access_control',
        executable='access_node', # 你的混沌測試器
        name='m3_access_control_node',
        output='screen',
        respawn=True,
        respawn_delay=2.0
    )

    m4_noise_node = Node(
        package='m4_environment_monitor',
        executable='noise_node',
        name='m4_noise_node',
        output='screen',
        respawn=True,
        respawn_delay=2.0
    )

    m5_aq_node = Node(
        package='m5_air_quality_monitor',
        executable='aq_node',
        name='m5_aq_node',
        output='screen',
        respawn=True,
        respawn_delay=2.0
    )

    #m6_display_node = Node(
    #    package='m6_local_display',
    #    executable='display_node',
    #    name='m6_display_node',
    #    output='screen',
    #    respawn=True,
    #    respawn_delay=2.0
    #)

    it_gateway_node = Node(
        package='v5_it_gateway',
        executable='it_gateway_node',
        name='it_gateway_node',
        output='screen',
        respawn=True,
        respawn_delay=5.0 # 給雲端連線較長的重試喘息時間
    )

    # ========================================================================
    # 事件驅動波次啟動邏輯 (Day 1 Bring-up)
    # ========================================================================

    # Wave 1: 當 OT Gateway 成功啟動後，觸發 Bridge Node
    wave_1_handler = RegisterEventHandler(
        event_handler=OnProcessStart(
            target_action=ot_gateway_node,
            on_start=[
                LogInfo(msg="🌊 [Wave 1] OT Gateway 啟動成功，物理底盤已就緒。正在喚醒領域大腦 (Bridge Node)..."),
                bridge_node
            ]
        )
    )

    # Wave 2: 當 Bridge Node 成功啟動後，並發啟動所有邊緣微服務
    wave_2_handler = RegisterEventHandler(
        event_handler=OnProcessStart(
            target_action=bridge_node,
            on_start=[
                LogInfo(msg="🌊 [Wave 2] 領域大腦甦醒。展開內部微服務 (M3~M6)..."),
                m3_access_node,
                m4_noise_node,
                m5_aq_node,
            ]
        )
    )

    # Wave 3: 當最後一個微服務 (M5) 啟動後，延遲 3 秒才放行 IT Gateway
    # (結合事件與 TimerAction：確保系統有 3 秒鐘的時間完成初次通訊與狀態收斂，再對外連線)
    wave_3_handler = RegisterEventHandler(
        event_handler=OnProcessStart(
            target_action=m5_aq_node,
            on_start=[
                TimerAction(
                    period=3.0,
                    actions=[
                        LogInfo(msg="🌊 [Wave 3] 邊緣系統狀態已收斂。放行 IT Gateway 連接 Firebase！"),
                        it_gateway_node
                    ]
                )
            ]
        )
    )

    # ========================================================================
    # 回傳 LaunchDescription
    # ========================================================================
    return LaunchDescription([
        # 整個骨牌效應的第一張牌：啟動 OT Gateway
        LogInfo(msg="🌊 [Wave 0] 系統啟動，準備掛載底層硬體資源..."),
        ot_gateway_node,
        
        # 註冊所有波次的事件監聽器
        wave_1_handler,
        wave_2_handler,
        wave_3_handler
    ])