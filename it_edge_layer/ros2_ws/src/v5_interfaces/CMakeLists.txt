cmake_minimum_required(VERSION 3.8)
project(v5_interfaces)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 1. 尋找 ROS 2 核心建置系統
find_package(ament_cmake REQUIRED)

# ==========================================
# 介面產生器設定
# ==========================================
# 2. 尋找把 .msg 翻譯成 C++/Python 的轉譯套件
find_package(rosidl_default_generators REQUIRED)

# 3. 告訴系統：請幫我編譯這個指定的 .msg 檔案！
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/SafetyState.msg"
)
# ==========================================

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

# 永遠在最後一行宣告套件打包完成
ament_package()