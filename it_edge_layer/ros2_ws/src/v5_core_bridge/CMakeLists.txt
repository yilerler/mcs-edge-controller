cmake_minimum_required(VERSION 3.8)
project(v5_core_bridge)

# ==========================================================
# 🚀 現代 C++ 武裝 (Modern C++ Support)
# ROS 2 Jazzy 原生支援 C++20，我們毫不猶豫地開啟它！
# ==========================================================
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 強制使用 C++20 標準
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==========================================================
# 📦 尋找 ROS 2 依賴套件
# ==========================================================
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(v5_interfaces REQUIRED) 

# ==========================================================
# 🏗️ 定義執行檔 (Executable)
# ==========================================================
add_executable(bridge_node src/bridge_node.cpp)
ament_target_dependencies(bridge_node rclcpp std_msgs v5_interfaces) 

# ==========================================================
# 🔗 核心橋接：Monorepo 跨界蟲洞 (Cross-Boundary Include)
# 告訴編譯器往上跳四層，去 OT 層尋找 v5_ioctl_contract.h
# 路徑推算：v5_core_bridge -> src -> ros2_ws -> it_edge_layer -> (根目錄) -> ot_defense_layer
# ==========================================================
target_include_directories(bridge_node PRIVATE
  "../../../../ot_defense_layer/include"
)

# 連結 ROS 2 的函式庫
ament_target_dependencies(bridge_node rclcpp std_msgs)

# ==========================================================
# 🚚 安裝規則 (Install Rules)
# 告訴 colcon 把編譯好的執行檔放在哪裡，這樣 ros2 run 才能找到
# ==========================================================
install(TARGETS
  bridge_node
  DESTINATION lib/${PROJECT_NAME}
)

# ==========================================================
# 🏁 結束宣告
# ==========================================================
ament_package()