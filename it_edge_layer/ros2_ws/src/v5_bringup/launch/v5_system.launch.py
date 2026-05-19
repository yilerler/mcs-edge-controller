from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(package='v5_core_bridge', executable='bridge_node', name='v5_core_bridge_node', output='screen', respawn=True, respawn_delay=2.0),
        Node(package='m4_environment_monitor', executable='noise_node', name='m4_noise_node', output='log', respawn=True),
        Node(package='m5_air_quality_monitor', executable='aq_node', name='m5_aq_node', output='log', respawn=True),
        Node(package='m7_cloud_forwarder', executable='forwarder_node', name='m7_forwarder_node', output='screen', respawn=True)
    ])