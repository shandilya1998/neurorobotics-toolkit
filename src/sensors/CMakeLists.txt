cmake_minimum_required(VERSION 3.8)
project(sensors)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(image_transport REQUIRED)
find_package(OpenCV 4.2 REQUIRED)
find_package(PkgConfig REQUIRED)
# Command to check gstreamer dependencies
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-video-1.0  gstreamer-app-1.0)

include_directories(include)
link_directories( ${GST_LIBRARY_DIRS}  )
include_directories( ${GST_INCLUDE_DIRS}  )

include_directories(${OpenCV_INCLUDE_DIRS}) # include headers for opencv in all executables

set(THIS_PACKAGE_INCLUDE_DEPENDS
	rclcpp
	sensor_msgs
	cv_bridge
	image_transport
	std_msgs
)

add_executable(video_logger_node
	src/temp.cpp
	src/video/logger.cpp
)

ament_target_dependencies(video_logger_node
	PUBLIC ${THIS_PACKAGE_INCLUDE_DEPENDS}
)

target_link_libraries(video_logger_node
	PUBLIC ${OpenCV_LIBS}
	PUBLIC ${GST_LIBRARIES}
)

install (DIRECTORY include/ 
	DESTINATION include
)

install(TARGETS video_logger_node
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION lib/${PROJECT_NAME}
	INCLUDES DESTINATION include/${PROJECT_NAME}
)

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
