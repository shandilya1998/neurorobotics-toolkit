#include <gst/app/gstappsrc.h>
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

class VideoLogger{
	public:
		VideoLogger(rclcpp::Logger logger, std::string &outputPath, VideoProperties &props);
		~VideoLogger();
		int getVideoLoggerStatus(){return mpVideoLoggerStatus;}
		void logFrame(cv::Mat &image);
	private:
		GstElement* mpPipeline;
		GstElement* mpAppsrc;
		GstElement* mpVideoconvert;
		GstElement* mpX264enc;
		GstElement* mpMp4mux;
		GstElement* mpFilesink;
		GstMapInfo mpMap;
		
		std::string mpOutputFilePath;
		int mpOutputImageWidth;
		int mpOutputImageHeight;
		int mpNumChannels;
		int mpInputDataFPS;
		int mpOutputFrameSize;

		int mpVideoLoggerStatus = 0;
		int mpFrameCounter;

		rclcpp::Logger mpLogger;
};

class VideoLoggerNode: public rclcpp::Node{
	public:
		VideoLoggerNode(std::string nodeName);
		void frameCallback(const sensor_msgs::msg::Image::ConstSharedPtr &img);

	private:
		std::string mpOutputFilePath;
		int mpOutputImageWidth;
		int mpOutputImageHeight;
		int mpInputDataFPS;
		std::string mpTopicName;
		std::string mpImageType;
		int mpNumChannels;

		rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr mpFrameSubscriber;

		std::unique_ptr<VideoLogger> mpVideoLogger;
};
