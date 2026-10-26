cmake_minimum_required(VERSION 3.8)
project(communication)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)

# Daftarkan executable untuk sender
add_executable(udp_sender src/udp_sender.cpp)
ament_target_dependencies(udp_sender rclcpp)

# Daftarkan executable untuk receiver
add_executable(udp_receiver src/udp_receiver.cpp)
ament_target_dependencies(udp_receiver rclcpp)

# Install kedua executable
install(TARGETS
  udp_sender
  udp_receiver
  DESTINATION lib/${PROJECT_NAME}
)

# Install direktori launch
install(DIRECTORY launch/
  DESTINATION share/${PROJECT_NAME}/launch
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()