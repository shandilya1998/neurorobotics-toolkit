#include "sensors/video/reader.h"

int main(int argc, char **argv){
	rclcpp::init(argc, argv);

	std::shared_ptr<VideoReaderNode> node = std::make_shared<VideoReaderNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
