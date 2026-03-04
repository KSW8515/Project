cmake_minimum_required(VERSION 3.8)
project(nav_qt)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Network)
# uncomment the following section in order to fill in
# further dependencies manually.
# find_package(<dependency> REQUIRED)

# Set Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Build
add_executable(nav_qt
	src/mainwidget.cpp
	src/mainwidget.h
	src/main.cpp
	src/rosnode.cpp
	src/rosnode.h
	src/mainwidget.ui
	resources.qrc
)
ament_target_dependencies(nav_qt
	rclcpp geometry_msgs std_msgs sensor_msgs nav_msgs
)
		
target_link_libraries(nav_qt
	Qt6::Widgets
	Qt6::Network
)

target_include_directories(nav_qt PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Install
Install(TARGETS
	nav_qt
	DESTINATION lib/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
