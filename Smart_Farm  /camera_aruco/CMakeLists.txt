cmake_minimum_required(VERSION 3.8)
project(camera_aruco)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 1.
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)

# 2.
add_executable(camera_aruco_node src/camera_aruco_node.cpp)

# 3.
target_include_directories(camera_aruco_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${OpenCV_INCLUDE_DIRS}
  ${cv_bridge_INCLUDE_DIRS}
)

# 4.
# target_link_libraries
target_link_libraries(camera_aruco_node ${OpenCV_LIBRARIES})

ament_target_dependencies(camera_aruco_node
  rclcpp
  sensor_msgs
  geometry_msgs
  cv_bridge
)

# 5
install(TARGETS
  camera_aruco_node
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
