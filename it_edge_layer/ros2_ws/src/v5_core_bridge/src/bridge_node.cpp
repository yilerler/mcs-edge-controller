#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "v5_interfaces/msg/safety_state.hpp"
#include "v5_ioctl_contract.h"

using namespace std::chrono_literals;

// 🌟 [V5.2.1 重構] 
// 這裡原本有 enum class SystemState 與 DoorState 的定義，現在已經被徹底抹除！
// FSM 大腦將直接使用 v5_ioctl_contract.h 中的 V5_ 巨集。

class V5CoreBridgeNode : public rclcpp::Node {
public:
    V5CoreBridgeNode() : Node("v5_core_bridge_node"), fd_(-1) {
        memset(&contract_, 0, sizeof(contract_));

        fd_ = open("/dev/v5_safety_core", O_RDWR);
        if (fd_ < 0) {
            RCLCPP_WARN(this->get_logger(), "⚠️ 無法開啟 /dev/v5_safety_core！進入純軟體模擬模式。");
        }

        last_m4_time_ = this->now();
        last_m5_time_ = this->now();

        telemetry_pub_ = this->create_publisher<v5_interfaces::msg::SafetyState>("safety/semantic_state", 10);

        command_sub_ = this->create_subscription<std_msgs::msg::String>(
            "access/door_request", 10, std::bind(&V5CoreBridgeNode::command_callback, this, std::placeholders::_1));

        noise_sub_ = this->create_subscription<v5_interfaces::msg::SafetyState>(
            "environment/noise", 10, std::bind(&V5CoreBridgeNode::noise_callback, this, std::placeholders::_1));

        air_quality_sub_ = this->create_subscription<v5_interfaces::msg::SafetyState>(
            "environment/air_quality", 10, std::bind(&V5CoreBridgeNode::aq_callback, this, std::placeholders::_1));

        timer_ = this->create_wall_timer(50ms, std::bind(&V5CoreBridgeNode::fsm_tick, this));
    }

    ~V5CoreBridgeNode() {
        if (fd_ >= 0) close(fd_);
    }

private:
    void noise_callback(const v5_interfaces::msg::SafetyState::SharedPtr msg) {
        last_m4_time_ = this->now();
        current_noise_ = msg->noise_db; 
    }

    void aq_callback(const v5_interfaces::msg::SafetyState::SharedPtr msg) {
        last_m5_time_ = this->now();
        current_pm25_ = msg->pm25; 
    }

    void command_callback(const std_msgs::msg::String::SharedPtr msg) {
        if (msg->data == "OPEN") {
            door_state_ = V5_DOOR_PENDING; // 🌟 直通底層字典
            RCLCPP_INFO(this->get_logger(), "💳 收到刷卡請求，進入 AUTH_PENDING 審核狀態...");
        } else if (msg->data == "CLOSE") {
            door_state_ = V5_DOOR_LOCKED;
            contract_.it_door_request = V5_DOOR_LOCKED;
        }
    }

    void fsm_tick() {
        if (fd_ >= 0) {
            ioctl(fd_, V5_IOC_EXCHANGE, &contract_);
        } else {
            // 軟體模擬：預設給予常態營運巨集
            contract_.ot_system_level = V5_STATE_NORMAL; 
            contract_.fence_distance = 150.0;
        }

        auto now = this->now();
        bool m4_offline = (now - last_m4_time_).seconds() > 3.0;
        bool m5_offline = (now - last_m5_time_).seconds() > 3.0;

        // 🌟 [V5.2.1 重構] 巨觀狀態裁決：不再需要猜測數字，代碼即文件
        if (contract_.ot_system_level == V5_STATE_EMERGENCY) {
            sys_state_ = V5_STATE_EMERGENCY;
        } else if (contract_.ot_system_level == V5_STATE_WARNING) {
            sys_state_ = V5_STATE_WARNING;
        } else if (m4_offline || m5_offline) {
            sys_state_ = V5_STATE_DEGRADED;
        } else if (current_noise_ > 85.0 || current_pm25_ > 150.0) {
            sys_state_ = V5_STATE_WARNING;
        } else {
            sys_state_ = V5_STATE_NORMAL;
        }

        // 大門邏輯裁決
        if (sys_state_ == V5_STATE_EMERGENCY) {
            door_state_ = V5_DOOR_FORCE_RELEASED;
            contract_.it_door_request = V5_DOOR_FORCE_RELEASED;
        } else {
            if (door_state_ == V5_DOOR_PENDING) {
                if (sys_state_ == V5_STATE_NORMAL) {
                    door_state_ = V5_DOOR_GRANTED;
                    contract_.it_door_request = V5_DOOR_GRANTED;
                    RCLCPP_INFO(this->get_logger(), "✅ FSM 核准：系統正常，准許開門。");
                } else {
                    door_state_ = V5_DOOR_LOCKED;
                    contract_.it_door_request = V5_DOOR_LOCKED;
                    RCLCPP_WARN(this->get_logger(), "❌ FSM 攔截：系統狀態異常，拒絕開門！");
                }
            }
        }

        publish_semantic_state();
    }

    void publish_semantic_state() {
        auto msg = v5_interfaces::msg::SafetyState();

        // 🌟🌟🌟 [V5.2.1 終極紅利] 
        // 以前這裡有 20 多行的 if-else 在把 enum 轉成 0, 1, 2, 3。
        // 現在因為 C 巨集 (V5_STATE_*) 與 ROS 2 常數 (STATE_*) 的底層數值已經完美對齊，
        // 我們可以直接做 O(1) 的記憶體賦值！這就是「合約對齊」帶來的極致效能與整潔。
        msg.system_state = sys_state_;
        msg.door_state = door_state_;

        msg.noise_db = current_noise_;
        msg.pm25 = current_pm25_;
        msg.fence_distance = contract_.fence_distance;

        telemetry_pub_->publish(msg);
    }

    // ==========================================================
    // 變數宣告區
    // ==========================================================
    int fd_;
    v5_ioctl_contract_t contract_;
    
    // 🌟 將狀態機變數型別改為 uint8_t，完美承接 V5_ 巨集
    uint8_t sys_state_ = V5_STATE_NORMAL;
    uint8_t door_state_ = V5_DOOR_LOCKED;
    
    double current_noise_ = 60.0;
    double current_pm25_ = 20.0;
    
    rclcpp::Time last_m4_time_;
    rclcpp::Time last_m5_time_;

    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Publisher<v5_interfaces::msg::SafetyState>::SharedPtr telemetry_pub_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr command_sub_;
    rclcpp::Subscription<v5_interfaces::msg::SafetyState>::SharedPtr noise_sub_;
    rclcpp::Subscription<v5_interfaces::msg::SafetyState>::SharedPtr air_quality_sub_;
};

int main(int argc, char * argv[]) {
    rclcpp::init(argc, argv);
    try {
        rclcpp::spin(std::make_shared<V5CoreBridgeNode>());
    } catch (const std::exception &e) {
        RCLCPP_FATAL(rclcpp::get_logger("rclcpp"), "節點崩潰: %s", e.what());
    }
    rclcpp::shutdown();
    return 0;
}