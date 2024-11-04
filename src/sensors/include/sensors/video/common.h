#include <gst/gst.h>
#include <opencv2/opencv.hpp>
#include <rclcpp/rclcpp.hpp>
#include "sensor_msgs/msg/image.hpp"
#include <cv_bridge/cv_bridge.h>
#include <memory>

typedef struct {
	int mWidth;
	int mHeight;
	int mNumChannels;
	int mFPS;
} VideoProperties;
   
void check_element_creation(GstElement *element, const std::string &name);
void check_linking(bool success, const std::string &linking);
