#include "sensors/video/logger.h"

int main(int argc, char **argv){
	rclcpp::init(argc, argv);

	std::shared_ptr<VideoLoggerNode> node = std::make_shared<VideoLoggerNode>("video_logger_test");
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
