#include "rclcpp/rclcpp.hpp"
#include "v5_interfaces/msg/ot_state.hpp"

#include <chrono>
#include <memory>
#include <functional>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

// 🌟 引入 12 Bytes 的合約
#include "v5_ot_gateway/v5_ioctl_contract.h" 

using namespace std::chrono_literals;

class OTGatewayNode : public rclcpp::Node {
public:
    OTGatewayNode() : Node("ot_gateway_node") {
        fd_ = open("/dev/v5_safety_core", O_RDWR);
        if (fd_ < 0) {
            RCLCPP_ERROR(this->get_logger(), "【南向告警】無法開啟 /dev/v5_safety_core，進入虛擬模擬模式！");
        } else {
            RCLCPP_INFO(this->get_logger(), "【南向初始化】實體硬體渠道建立成功 (fd: %d)", fd_);
        }

        ot_pub_ = this->create_publisher<v5_interfaces::msg::OTState>("/v5/ot_state", 10);
        timer_ = this->create_wall_timer(
            std::chrono::milliseconds(100),
            std::bind(&OTGatewayNode::poll_and_publish, this));
    }

    ~OTGatewayNode() {
        if (fd_ >= 0) close(fd_);
    }

private:
    int fd_ = -1;
    rclcpp::Publisher<v5_interfaces::msg::OTState>::SharedPtr ot_pub_;
    rclcpp::TimerBase::SharedPtr timer_;

    void poll_and_publish() {
        v5_ioctl_contract_t raw_data;
        auto out_msg = v5_interfaces::msg::OTState();
        out_msg.header.stamp = this->now();

        if (fd_ >= 0 && ioctl(fd_, V5_IOC_EXCHANGE, &raw_data) == 0) {
            // 實體硬體讀取成功 ➔ 精準映射 12 Bytes Kernel 欄位
            out_msg.ot_system_level = raw_data.ot_system_level;
            out_msg.fence_status = raw_data.fence_status;
            out_msg.fence_distance = raw_data.fence_distance;
            out_msg.fire_heat_value = raw_data.fire_heat_value;
            out_msg.ot_heartbeat_ms = raw_data.ot_heartbeat_ms;
            
            // 🌟 [解耦完成] 再也不需要處理 RFID 與 門禁狀態！
        } else {
            // 模擬模式保底 
            out_msg.ot_system_level = V5_STATE_NORMAL;
            out_msg.fence_status = V5_FENCE_CLEAR;
            out_msg.fence_distance = 1000; 
            out_msg.fire_heat_value = 25;  
            out_msg.ot_heartbeat_ms = 0;
        }

        ot_pub_->publish(out_msg);
    }
};

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<OTGatewayNode>());
    rclcpp::shutdown();
    return 0;
}