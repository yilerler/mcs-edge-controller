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
// 🧠 [V5.2.3 核心重構] 絕對純淨的 FSM 邏輯沙盒
// ==========================================================
class V5SafetyFSM {
public:
    static void evaluate(
        uint32_t hardware_error_flags, // 🌟 改由 OTState 傳入硬體健康度
        double current_noise, 
        double current_pm25, 
        bool m4_offline, 
        bool m5_offline,
        uint8_t& out_sys_state,
        uint8_t& out_door_state) 
    {
        // 1. 巨觀防禦裁決 (決定論：優先權由高到低)
        if (hardware_error_flags > 0) {
            // 如果底層 OT 轉接器回報硬體異常，直接升級為緊急狀態
            out_sys_state = v5_interfaces::msg::SafetyState::STATE_EMERGENCY;
        } else if (m4_offline || m5_offline) {
            out_sys_state = v5_interfaces::msg::SafetyState::STATE_DEGRADED;
        } else if (current_noise > 85.0 || current_pm25 > 150.0) {
            out_sys_state = v5_interfaces::msg::SafetyState::STATE_WARNING;
        } else {
            out_sys_state = v5_interfaces::msg::SafetyState::STATE_NORMAL;
        }

        // 2. 邊界存取裁決 (門禁控制)
        if (out_sys_state == v5_interfaces::msg::SafetyState::STATE_EMERGENCY) {
            // 霸王條款：無條件釋放門鎖。
            // 🌟 大腦只負責做決定，不再負責寫入 ioctl。
            out_door_state = v5_interfaces::msg::SafetyState::DOOR_FORCE_RELEASED;
        } else {
            // 常規狀態審核
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
// 🌟 手術完成：成為一個完全無通訊層負擔的純 ROS 2 轉發節點
// ==========================================================
class V5CoreBridgeNode : public rclcpp::Node {
public:
    V5CoreBridgeNode() : Node("v5_core_bridge_node") {
        // 🌟 刪除：所有 open("/dev/...") 以及 fd_ 的相關邏輯

        last_m4_time_ = this->now();
        last_m5_time_ = this->now();

        intent_sub_ = this->create_subscription<v5_interfaces::msg::CloudIntent>(
            "/v5/cloud_intent", 10, std::bind(&V5CoreBridgeNode::intent_callback, this, std::placeholders::_1));

        // 1. 輸出插座：發布語意狀態給 M7 雲端模組
        telemetry_pub_ = this->create_publisher<v5_interfaces::msg::SafetyState>("safety/semantic_state", 10);

        // 2. 輸入插座：訂閱南向 OT 轉接器送來的物理事實
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

    // 🌟 刪除：解構子裡面的 close(fd_) 已經不需要了

private:
    // 🌟 新增：南向 OT 狀態的回調函數
    void ot_callback(const v5_interfaces::msg::OTState::SharedPtr msg) {
        // 將底層物理狀態快取，供大腦 50ms 的 Tick 迴圈使用
        latest_ot_error_flags_ = msg->hardware_error_flags;
        // 如果未來有 fence_distance，也是在這裡更新
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

    // 實作意圖驗證沙盒 (Intent Sandbox Callback)
    void intent_callback(const v5_interfaces::msg::CloudIntent::SharedPtr msg) {
        RCLCPP_INFO(this->get_logger(), "📥 [雲端沙盒] 收到新配置意圖。目標 Hash: %s", msg->intent_version_hash.c_str());

        // 【防禦核心邏輯】：否決權 (Veto Power)
        // 絕對不允許在火警（緊急狀態）時更新參數！
        if (sys_state_ == v5_interfaces::msg::SafetyState::STATE_EMERGENCY) {
            RCLCPP_WARN(this->get_logger(), "🛑 [意圖否決] 系統正處於緊急狀態，拒絕套用新配置！");
            return; // 直接丟棄意圖，保護生命安全
        }

        // 【邊界驗證】：物理極限檢查
        if (msg->desired_pm25_threshold < 10.0 || msg->desired_pm25_threshold > 900.0) {
            RCLCPP_WARN(this->get_logger(), "❌ [意圖否決] 期望 PM2.5 閾值 (%.1f) 超出物理安全極限，拒絕套用！", msg->desired_pm25_threshold);
            return;
        }

        // 【原子交換 Atomic Swap】：驗證通過，正式套用
        active_config_hash_ = msg->intent_version_hash;
        active_pm25_threshold_ = msg->desired_pm25_threshold;
        
        RCLCPP_INFO(this->get_logger(), "✅ [意圖套用] 成功切換至新配置版本: %s", active_config_hash_.c_str());
    }

    void fsm_tick() {
        // --------------------------------------------------
        // Phase 1: 準備防禦參數 (由 OT 訂閱取代 I/O 讀取)
        // --------------------------------------------------
        auto now = this->now();
        bool m4_offline = (now - last_m4_time_).seconds() > 3.0;
        bool m5_offline = (now - last_m5_time_).seconds() > 3.0;

        // --------------------------------------------------
        // Phase 2: 🌟 呼叫大腦 (Pure Logic Sandbox) 🌟
        // --------------------------------------------------
        V5SafetyFSM::evaluate(
            latest_ot_error_flags_, // 餵入來自南向轉接器的健康資訊
            current_noise_, current_pm25_, 
            m4_offline, m5_offline, 
            sys_state_, door_state_
        );

        // 防禦 Log 紀錄
        static uint8_t last_door_state = v5_interfaces::msg::SafetyState::DOOR_LOCKED;
        if (door_state_ != last_door_state) {
            if (door_state_ == v5_interfaces::msg::SafetyState::DOOR_GRANTED) {
                RCLCPP_INFO(this->get_logger(), "✅ FSM 核准：系統正常，准許開門。");
            } else if (door_state_ == v5_interfaces::msg::SafetyState::DOOR_LOCKED && last_door_state == v5_interfaces::msg::SafetyState::DOOR_PENDING) {
                RCLCPP_WARN(this->get_logger(), "❌ FSM 攔截：系統狀態異常，拒絕開門！");
            }
            last_door_state = door_state_;
        }
        // TODO [v5.2.4 沙盒深化]: FSM_LOGIC - 將 active_pm25_threshold_ 正式傳入 V5SafetyFSM::evaluate 簽章。
        // 說明: 目前為了快速驗證 OTA-C，動態閾值攔截暫時實作於大腦外殼 (V5CoreBridgeNode) 中。
        //       未來應修改 V5SafetyFSM 類別的 evaluate 方法，讓底層純數學沙盒完全接管
        //       所有動態環境參數的邊界運算與降級邏輯。
        if (current_pm25_ > active_pm25_threshold_) {
            sys_state_ = v5_interfaces::msg::SafetyState::STATE_WARNING;
            RCLCPP_WARN(this->get_logger(), "⚠️ 動態閾值觸發！當前 PM2.5 (%.1f) > 閾值 (%.1f)", current_pm25_, active_pm25_threshold_);
        } else {
             // 呼叫原本的 FSM
            V5SafetyFSM::evaluate(latest_ot_error_flags_, current_noise_, current_pm25_, m4_offline, m5_offline, sys_state_, door_state_);
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
        
        // v5.2.3 預留防禦溯源欄位
        msg.current_config_version = active_config_hash_;
        msg.override_source = "FSM_LOGIC";
        
        telemetry_pub_->publish(msg);
    }

    // ==========================================================
    // 變數宣告區 (刪除了 fd_ 與 contract_)
    // ==========================================================
    uint32_t latest_ot_error_flags_ = 0;
    uint8_t sys_state_ = v5_interfaces::msg::SafetyState::STATE_NORMAL;
    uint8_t door_state_ = v5_interfaces::msg::SafetyState::DOOR_LOCKED;
    
    std::string active_config_hash_ = "v5.2.3_base";
    double active_pm25_threshold_ = 150.0;
    rclcpp::Subscription<v5_interfaces::msg::CloudIntent>::SharedPtr intent_sub_;

    double current_noise_ = 60.0;
    double current_pm25_ = 20.0;
    
    rclcpp::Time last_m4_time_;
    rclcpp::Time last_m5_time_;

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