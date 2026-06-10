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
// 🧠 [V5.2.2 核心重構] 絕對純淨的 FSM 邏輯沙盒
// 這裡沒有 ioctl，沒有 ROS 2，只有 ICD 數位合約與純粹的防禦數學。
// 未來移植到 Pico W (FreeRTOS) 時，這整個 Class 可以 1:1 直接複製貼上！
// ==========================================================
class V5SafetyFSM {
public:
    static void evaluate(
        v5_ioctl_contract_t& contract, 
        double current_noise, 
        double current_pm25, 
        bool m4_offline, 
        bool m5_offline,
        uint8_t& out_sys_state,
        uint8_t& out_door_state) 
    {
        // 1. 巨觀防禦裁決 (決定論：優先權由高到低)
        if (contract.ot_system_level == V5_STATE_EMERGENCY) {
            out_sys_state = V5_STATE_EMERGENCY;
        } else if (contract.ot_system_level == V5_STATE_WARNING) {
            out_sys_state = V5_STATE_WARNING;
        } else if (m4_offline || m5_offline) {
            out_sys_state = V5_STATE_DEGRADED;
        } else if (current_noise > 85.0 || current_pm25 > 150.0) {
            out_sys_state = V5_STATE_WARNING;
        } else {
            out_sys_state = V5_STATE_NORMAL;
        }

        // 2. 邊界存取裁決 (門禁控制)
        if (out_sys_state == V5_STATE_EMERGENCY) {
            // 霸王條款：無條件釋放門鎖，並將指令寫入 ICD 要求 OT 執行
            out_door_state = V5_DOOR_FORCE_RELEASED;
            contract.it_door_request = V5_DOOR_FORCE_RELEASED;
        } else {
            // 常規狀態審核
            if (out_door_state == V5_DOOR_PENDING) {
                if (out_sys_state == V5_STATE_NORMAL) {
                    out_door_state = V5_DOOR_GRANTED;
                    contract.it_door_request = V5_DOOR_GRANTED;
                } else {
                    out_door_state = V5_DOOR_LOCKED;
                    contract.it_door_request = V5_DOOR_LOCKED;
                }
            }
        }
    }
};

// ==========================================================
// 🛡️ 基礎設施外殼 (Infrastructure Shell)
// 職責：處理 Linux 系統呼叫 (Dirty I/O) 與 ROS 2 網路通訊
// ==========================================================
class V5CoreBridgeNode : public rclcpp::Node {
public:
    V5CoreBridgeNode() : Node("v5_core_bridge_node"), fd_(-1) {
        memset(&contract_, 0, sizeof(contract_));

        fd_ = open("/dev/v5_safety_core", O_RDWR);
        if (fd_ < 0) {
            RCLCPP_WARN(this->get_logger(), "⚠️ 無法開啟 /dev/v5_safety_core！進入全模擬沙盒模式。");
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
            door_state_ = V5_DOOR_PENDING; 
            RCLCPP_INFO(this->get_logger(), "💳 收到刷卡請求，進入 PENDING 審核狀態...");
        } else if (msg->data == "CLOSE") {
            door_state_ = V5_DOOR_LOCKED;
            contract_.it_door_request = V5_DOOR_LOCKED;
        }
    }

    void fsm_tick() {
        // --------------------------------------------------
        // Phase 1: 讀取硬體狀態 (I/O Read)
        // --------------------------------------------------
        if (fd_ >= 0) {
            ioctl(fd_, V5_IOC_EXCHANGE, &contract_);
        } else {
            // Mock as a Feature: 軟體模擬 ICD 狀態
            contract_.ot_system_level = V5_STATE_NORMAL; 
            contract_.fence_distance = 150.0;
        }

        // --------------------------------------------------
        // Phase 2: 準備防禦參數
        // --------------------------------------------------
        auto now = this->now();
        bool m4_offline = (now - last_m4_time_).seconds() > 3.0;
        bool m5_offline = (now - last_m5_time_).seconds() > 3.0;

        // --------------------------------------------------
        // Phase 3: 🌟 呼叫大腦 (Pure Logic Sandbox) 🌟
        // --------------------------------------------------
        V5SafetyFSM::evaluate(
            contract_, 
            current_noise_, current_pm25_, 
            m4_offline, m5_offline, 
            sys_state_, door_state_
        );

        // 如果門禁狀態改變，在此紀錄防禦 Log
        static uint8_t last_door_state = V5_DOOR_LOCKED;
        if (door_state_ != last_door_state) {
            if (door_state_ == V5_DOOR_GRANTED) {
                RCLCPP_INFO(this->get_logger(), "✅ FSM 核准：系統正常，准許開門。");
            } else if (door_state_ == V5_DOOR_LOCKED && last_door_state == V5_DOOR_PENDING) {
                RCLCPP_WARN(this->get_logger(), "❌ FSM 攔截：系統狀態異常，拒絕開門！");
            }
            last_door_state = door_state_;
        }

        // --------------------------------------------------
        // Phase 4: 狀態發布與寫回硬體
        // --------------------------------------------------
        if (fd_ >= 0) {
            // 將 FSM 計算後的新合約 (含 it_door_request) 即時寫入 Kernel
            ioctl(fd_, V5_IOC_EXCHANGE, &contract_); 
        }
        publish_semantic_state();
    }

    void publish_semantic_state() {
        auto msg = v5_interfaces::msg::SafetyState();
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
