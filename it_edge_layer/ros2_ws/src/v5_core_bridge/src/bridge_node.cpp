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

// ==========================================================
// 1. Enum 宣告區
// ==========================================================
enum class SystemState {
    NORMAL_OPERATION, ACTIVE_WARNING, DEGRADED, EMERGENCY_LOCKDOWN
};

enum class DoorState {
    SECURE_LOCKED, AUTH_PENDING, ACCESS_GRANTED, FORCE_RELEASED
};

class V5CoreBridgeNode : public rclcpp::Node {
public:
    V5CoreBridgeNode() : Node("v5_core_bridge_node"), fd_(-1) {
        memset(&contract_, 0, sizeof(contract_));

        // 開啟底層 OT 核心通訊
        fd_ = open("/dev/v5_safety_core", O_RDWR);
        if (fd_ < 0) {
            RCLCPP_WARN(this->get_logger(), "⚠️ 無法開啟 /dev/v5_safety_core！進入純軟體模擬模式。");
        }

        // 初始化所有 Watchdog 時間戳
        last_m4_time_ = this->now();
        last_m5_time_ = this->now(); // 🌟 補回 M5

        // 發布者：強型別安全合約
        telemetry_pub_ = this->create_publisher<v5_interfaces::msg::SafetyState>("safety/semantic_state", 10);

        // 訂閱者：M3 門禁字串指令
        command_sub_ = this->create_subscription<std_msgs::msg::String>(
            "access/door_request", 10, std::bind(&V5CoreBridgeNode::command_callback, this, std::placeholders::_1));

        // 訂閱者：M4 噪音
        noise_sub_ = this->create_subscription<v5_interfaces::msg::SafetyState>(
            "environment/noise", 10, std::bind(&V5CoreBridgeNode::noise_callback, this, std::placeholders::_1));

        // 訂閱者：M5 空品 🌟 (補回)
        air_quality_sub_ = this->create_subscription<v5_interfaces::msg::SafetyState>(
            "environment/air_quality", 10, std::bind(&V5CoreBridgeNode::aq_callback, this, std::placeholders::_1));

        // 20Hz 系統心跳
        timer_ = this->create_wall_timer(50ms, std::bind(&V5CoreBridgeNode::fsm_tick, this));
    }

    ~V5CoreBridgeNode() {
        if (fd_ >= 0) close(fd_);
    }

private:
    // ==========================================================
    // 2. 感測器回呼區 (直接讀取 Struct，零延遲)
    // ==========================================================
    void noise_callback(const v5_interfaces::msg::SafetyState::SharedPtr msg) {
        last_m4_time_ = this->now();
        current_noise_ = msg->noise_db; 
    }

    void aq_callback(const v5_interfaces::msg::SafetyState::SharedPtr msg) {
        last_m5_time_ = this->now();
        current_pm25_ = msg->pm25; // 🌟 現在認得 current_pm25_ 了
    }

    void command_callback(const std_msgs::msg::String::SharedPtr msg) {
        if (msg->data == "OPEN") {
            door_state_ = DoorState::AUTH_PENDING;
            RCLCPP_INFO(this->get_logger(), "💳 收到刷卡請求，進入 AUTH_PENDING 審核狀態...");
        } else if (msg->data == "CLOSE") {
            door_state_ = DoorState::SECURE_LOCKED;
            contract_.it_door_request = 0;
        }
    }

    // ==========================================================
    // 3. 核心狀態機引擎
    // ==========================================================
    void fsm_tick() {
        // 與 OT 交換合約
        if (fd_ >= 0) {
            ioctl(fd_, V5_IOC_EXCHANGE, &contract_);
        } else {
            // 軟體模擬：給予預設值避免系統死鎖
            contract_.ot_system_level = 2; // NORMAL
            contract_.fence_distance = 150.0;
        }

        // Watchdog 斷線判定
        auto now = this->now();
        bool m4_offline = (now - last_m4_time_).seconds() > 3.0;
        bool m5_offline = (now - last_m5_time_).seconds() > 3.0;

        // 巨觀狀態裁決 (優先權由高到低)
        if (contract_.ot_system_level == 0) {
            sys_state_ = SystemState::EMERGENCY_LOCKDOWN;
        } else if (contract_.ot_system_level == 1) {
            sys_state_ = SystemState::ACTIVE_WARNING;
        } else if (m4_offline || m5_offline) {
            sys_state_ = SystemState::DEGRADED;
        } else if (current_noise_ > 85.0 || current_pm25_ > 150.0) {
            sys_state_ = SystemState::ACTIVE_WARNING;
        } else {
            sys_state_ = SystemState::NORMAL_OPERATION;
        }

        // 大門邏輯裁決
        if (sys_state_ == SystemState::EMERGENCY_LOCKDOWN) {
            door_state_ = DoorState::FORCE_RELEASED;
            contract_.it_door_request = 0;
        } else {
            if (door_state_ == DoorState::AUTH_PENDING) {
                if (sys_state_ == SystemState::NORMAL_OPERATION) {
                    door_state_ = DoorState::ACCESS_GRANTED;
                    contract_.it_door_request = 1;
                    RCLCPP_INFO(this->get_logger(), "✅ FSM 核准：系統正常，准許開門。");
                } else {
                    door_state_ = DoorState::SECURE_LOCKED;
                    contract_.it_door_request = 0;
                    RCLCPP_WARN(this->get_logger(), "❌ FSM 攔截：系統狀態異常，拒絕開門！");
                }
            }
        }

        // 🌟 修正：不帶參數完美呼叫
        publish_semantic_state();
    }

    // 🌟 修正：函數簽名不帶參數，直接使用內部變數
    void publish_semantic_state() {
        auto msg = v5_interfaces::msg::SafetyState();

        // 翻譯系統狀態
        if (sys_state_ == SystemState::EMERGENCY_LOCKDOWN) {
            msg.system_state = v5_interfaces::msg::SafetyState::STATE_EMERGENCY;
        } else if (sys_state_ == SystemState::ACTIVE_WARNING) {
            msg.system_state = v5_interfaces::msg::SafetyState::STATE_WARNING;
        } else if (sys_state_ == SystemState::DEGRADED) {
            msg.system_state = v5_interfaces::msg::SafetyState::STATE_DEGRADED;
        } else {
            msg.system_state = v5_interfaces::msg::SafetyState::STATE_NORMAL;
        }

        // 翻譯大門狀態
        if (door_state_ == DoorState::SECURE_LOCKED) {
            msg.door_state = v5_interfaces::msg::SafetyState::DOOR_LOCKED;
        } else if (door_state_ == DoorState::AUTH_PENDING) {
            msg.door_state = v5_interfaces::msg::SafetyState::DOOR_PENDING;
        } else if (door_state_ == DoorState::ACCESS_GRANTED) {
            msg.door_state = v5_interfaces::msg::SafetyState::DOOR_GRANTED;
        } else if (door_state_ == DoorState::FORCE_RELEASED) {
            msg.door_state = v5_interfaces::msg::SafetyState::DOOR_FORCE_RELEASED;
        }

        // 填入數值
        msg.noise_db = current_noise_;
        msg.pm25 = current_pm25_;
        msg.fence_distance = contract_.fence_distance;

        telemetry_pub_->publish(msg);
    }

    // ==========================================================
    // 4. 變數宣告區 🌟 (完整補回所有屬性)
    // ==========================================================
    int fd_;
    v5_ioctl_contract_t contract_;
    SystemState sys_state_ = SystemState::NORMAL_OPERATION;
    DoorState door_state_ = DoorState::SECURE_LOCKED;
    
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