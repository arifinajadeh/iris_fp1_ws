
cmake_minimum_required(VERSION 3.8)
project(vision)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)
find_package(std_msgs REQUIRED)

add_executable(bev_node src/bev_node.cpp)

ament_target_dependencies(bev_node
  rclcpp
  sensor_msgs
  cv_bridge
  std_msgs
)

target_link_libraries(bev_node ${OpenCV_LIBS})

install(TARGETS
  bev_node
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
