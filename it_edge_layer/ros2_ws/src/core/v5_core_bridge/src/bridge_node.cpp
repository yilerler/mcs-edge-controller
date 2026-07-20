#include <chrono>
#include <functional>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "v5_interfaces/msg/safety_state.hpp"
#include "v5_interfaces/msg/ot_state.hpp" 
#include "v5_interfaces/msg/cloud_intent.hpp"
using namespace std::chrono_literals;

// ==========================================================
// 🧠 [V5.2.4 核心重構] 六角形架構：領域策略引擎 (Domain Policy Engine)
// 原理：將 OT 傳來的物理事實與 IT 傳來的軟體遙測結合，推演全域狀態
// ==========================================================
class V5SafetyFSM {
public:
    static void evaluate(
        uint8_t ot_sys_level,          // 新增：底層硬體的全域狀態防線
        uint8_t fence_status,          // 新增：M2 電子圍籬物理狀態
        uint16_t fire_heat_value,      // 新增：實體火警溫度/特徵值
        double current_noise, 
        double current_pm25, 
        double active_pm25_threshold, 
        bool m4_offline, 
        bool m5_offline,
        uint8_t& out_sys_state,
        uint8_t& out_door_state) 
    {
        // 1. 巨觀防禦裁決 (由硬到軟，層層遞進)
        if (ot_sys_level == 3 || fire_heat_value > 80) { // 假設 80°C 為火災觸發點
            // 最高優先權：實體火警或底層核心發出 EMERGENCY (3)
            out_sys_state = v5_interfaces::msg::SafetyState::STATE_EMERGENCY;
        } else if (m4_offline || m5_offline) {
            // 次高優先權：IT 軟體感測器斷線，系統進入防禦性盲區降級
            out_sys_state = v5_interfaces::msg::SafetyState::STATE_DEGRADED;
        } else if (ot_sys_level == 1 || fence_status == 1 || 
                   current_noise > 85.0 || current_pm25 > active_pm25_threshold) {
            // 警告層級：圍籬有人闖入 (BRAKING=1)、底層發出警告 (1)、或環境數據超標
            out_sys_state = v5_interfaces::msg::SafetyState::STATE_WARNING;
        } else {
            out_sys_state = v5_interfaces::msg::SafetyState::STATE_NORMAL;
        }

        // 2. 邊界存取裁決 (門禁控制策略不變，依然聽令於巨觀防禦)
        if (out_sys_state == v5_interfaces::msg::SafetyState::STATE_EMERGENCY) {
            out_door_state = v5_interfaces::msg::SafetyState::DOOR_FORCE_RELEASED;
        } else {
            if (out_door_state == v5_interfaces::msg::SafetyState::DOOR_PENDING) {
                if (out_sys_state == v5_interfaces::msg::SafetyState::STATE_NORMAL) {
                    out_door_state = v5_interfaces::msg::SafetyState::DOOR_GRANTED;
                } else {
                    out_door_state = v5_interfaces::msg::SafetyState::DOOR_LOCKED;
                }
            }
        }
    }
};

// ==========================================================
// 🛡️ 基礎設施外殼 (Infrastructure Shell)
// ==========================================================
class V5CoreBridgeNode : public rclcpp::Node {
public:
    V5CoreBridgeNode() : Node("v5_core_bridge_node") {
        last_m4_time_ = this->now();
        last_m5_time_ = this->now();
        last_ot_heartbeat_time_ = this->now(); // 加入對南向硬體的心跳監控

        intent_sub_ = this->create_subscription<v5_interfaces::msg::CloudIntent>(
            "/v5/cloud_intent", 10, std::bind(&V5CoreBridgeNode::intent_callback, this, std::placeholders::_1));

        telemetry_pub_ = this->create_publisher<v5_interfaces::msg::SafetyState>("safety/semantic_state", 10);

        ot_sub_ = this->create_subscription<v5_interfaces::msg::OTState>(
            "/v5/ot_state", 10, std::bind(&V5CoreBridgeNode::ot_callback, this, std::placeholders::_1));

        command_sub_ = this->create_subscription<std_msgs::msg::String>(
            "access/door_request", 10, std::bind(&V5CoreBridgeNode::command_callback, this, std::placeholders::_1));

        noise_sub_ = this->create_subscription<v5_interfaces::msg::SafetyState>(
            "environment/noise", 10, std::bind(&V5CoreBridgeNode::noise_callback, this, std::placeholders::_1));

        air_quality_sub_ = this->create_subscription<v5_interfaces::msg::SafetyState>(
            "environment/air_quality", 10, std::bind(&V5CoreBridgeNode::aq_callback, this, std::placeholders::_1));

        timer_ = this->create_wall_timer(50ms, std::bind(&V5CoreBridgeNode::fsm_tick, this));
    }

private:
    // 🌟 全新南向突觸：接收真實物理事實
    void ot_callback(const v5_interfaces::msg::OTState::SharedPtr msg) {
        latest_ot_sys_level_ = msg->ot_system_level;
        latest_fence_status_ = msg->fence_status;
        latest_fire_heat_value_ = msg->fire_heat_value;
        last_ot_heartbeat_time_ = this->now(); // 更新硬體心跳時間戳記
    }

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
            door_state_ = v5_interfaces::msg::SafetyState::DOOR_PENDING; 
            RCLCPP_INFO(this->get_logger(), "💳 收到刷卡請求，進入 PENDING 審核狀態...");
        } else if (msg->data == "CLOSE") {
            door_state_ = v5_interfaces::msg::SafetyState::DOOR_LOCKED;
        }
    }

    void intent_callback(const v5_interfaces::msg::CloudIntent::SharedPtr msg) {
        if (sys_state_ == v5_interfaces::msg::SafetyState::STATE_EMERGENCY) {
            RCLCPP_WARN(this->get_logger(), "🛑 [意圖否決] 系統正處於緊急狀態，拒絕套用新配置！");
            return;
        }
        if (msg->desired_pm25_threshold < 10.0 || msg->desired_pm25_threshold > 900.0) {
            RCLCPP_WARN(this->get_logger(), "❌ [意圖否決] 期望 PM2.5 閾值 (%.1f) 超出物理安全極限，拒絕套用！", msg->desired_pm25_threshold);
            return;
        }
        active_config_hash_ = msg->intent_version_hash;
        active_pm25_threshold_ = msg->desired_pm25_threshold;
        RCLCPP_INFO(this->get_logger(), "✅ [意圖套用] 成功切換至新配置版本: %s", active_config_hash_.c_str());
    }

    void fsm_tick() {
        auto now = this->now();
        
        // 增加對底層硬體的心跳監控 (超過 1 秒沒收到 Kernel 資料視為斷線)
        bool ot_offline = (now - last_ot_heartbeat_time_).seconds() > 1.0;
        if (ot_offline) {
            latest_ot_sys_level_ = 2; // 強制注入 DEGRADED 狀態
        }

        bool m4_offline = (now - last_m4_time_).seconds() > 3.0;
        bool m5_offline = (now - last_m5_time_).seconds() > 3.0;

        // 🌟 將全新的物理事實餵入策略引擎
        V5SafetyFSM::evaluate(
            latest_ot_sys_level_, 
            latest_fence_status_,
            latest_fire_heat_value_,
            current_noise_, 
            current_pm25_, 
            active_pm25_threshold_, 
            m4_offline, 
            m5_offline, 
            sys_state_, 
            door_state_
        );

        static uint8_t last_door_state = v5_interfaces::msg::SafetyState::DOOR_LOCKED;
        if (door_state_ != last_door_state) {
            if (door_state_ == v5_interfaces::msg::SafetyState::DOOR_GRANTED) {
                RCLCPP_INFO(this->get_logger(), "✅ FSM 核准：系統正常，准許開門。");
            } else if (door_state_ == v5_interfaces::msg::SafetyState::DOOR_LOCKED && last_door_state == v5_interfaces::msg::SafetyState::DOOR_PENDING) {
                RCLCPP_WARN(this->get_logger(), "❌ FSM 攔截：系統狀態異常，拒絕開門！");
            }
            last_door_state = door_state_;
        }
        
        publish_semantic_state();
    }

    void publish_semantic_state() {
        auto msg = v5_interfaces::msg::SafetyState();
        msg.header.stamp = this->now();
        msg.system_state = sys_state_;
        msg.door_state = door_state_;
        msg.noise_db = current_noise_;
        msg.pm25 = current_pm25_;
        
        msg.current_config_version = active_config_hash_;
        msg.override_source = "FSM_LOGIC";
        
        telemetry_pub_->publish(msg);
    }

    // ==========================================================
    // 快取狀態變數
    // ==========================================================
    uint8_t latest_ot_sys_level_ = 0;
    uint8_t latest_fence_status_ = 0;
    uint16_t latest_fire_heat_value_ = 0;

    uint8_t sys_state_ = v5_interfaces::msg::SafetyState::STATE_NORMAL;
    uint8_t door_state_ = v5_interfaces::msg::SafetyState::DOOR_LOCKED;
    
    std::string active_config_hash_ = "v5.2.3_base";
    double active_pm25_threshold_ = 150.0;
    
    double current_noise_ = 60.0;
    double current_pm25_ = 20.0;
    
    rclcpp::Time last_m4_time_;
    rclcpp::Time last_m5_time_;
    rclcpp::Time last_ot_heartbeat_time_; // OT 斷線防禦機制

    rclcpp::Subscription<v5_interfaces::msg::CloudIntent>::SharedPtr intent_sub_;
    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Publisher<v5_interfaces::msg::SafetyState>::SharedPtr telemetry_pub_;
    rclcpp::Subscription<v5_interfaces::msg::OTState>::SharedPtr ot_sub_;
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