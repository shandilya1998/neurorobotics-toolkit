#include "sensors/video/common.h"
#include <gst/app/app.h>

struct Frame{
	public:
		cv::Mat frame;
		double timestamp;
		std::string format;
		int height, width;
};

class VideoReader {
public:
    VideoReader(rclcpp::Logger logger, const std::string& inputFilePath, std::function<void(std::shared_ptr<Frame>)> cb);
    ~VideoReader();
	void run();
	void frameCallback(std::shared_ptr<Frame> frame);

private:
	rclcpp::Logger mpLogger;
    static GstFlowReturn new_sample(GstAppSink *sink, gpointer user_data);
    gboolean bus_call(GstBus *bus, GstMessage *msg);
    
    GstElement *mpPipeline;
    GstElement *mpFilesrc;
    GstElement *mpDemuxer;
    GstElement *mpQueue;
    GstElement *mpH264Parser;
    GstElement *mpH264Decoder;
    GstElement *mpVideoconvert;
    GstElement *mpCapsfilter;
    GstElement *mpAppsink;

    cv::VideoWriter writer;
    GMainLoop *loop;

    std::string inputFilePath;
    std::string outputFilePath;

	std::function<void(std::shared_ptr<Frame>)> mpFrameCallback;
};

class VideoReaderNode: public rclcpp::Node{
	public:
		VideoReaderNode();
		void frameCallback(std::shared_ptr<Frame> frame);

	private:
		std::shared_ptr<VideoReader> mpReader;
		std::string mpInputFilePath;
		std::string mpCameraTopic;
		rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr mpFramePub;
};
