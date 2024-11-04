#include "sensors/video/logger.h"

VideoLogger::VideoLogger(rclcpp::Logger logger, std::string &outputPath, VideoProperties &props) : mpLogger(logger){
	mpOutputFilePath = outputPath;
	mpOutputImageWidth = props.mWidth;
	mpOutputImageHeight = props.mHeight;
	mpInputDataFPS = props.mFPS;
	mpOutputFrameSize = mpOutputImageWidth * mpOutputImageHeight * mpNumChannels;
	mpFrameCounter = 0;
	
    gst_init(nullptr, nullptr);
	// Create elements
    mpPipeline = gst_pipeline_new("video-logger-pipeline");
	mpAppsrc = gst_element_factory_make("appsrc", "mysource");
	mpVideoconvert = gst_element_factory_make("videoconvert", "myconvert");
	mpX264enc = gst_element_factory_make("x264enc", "myencoder");
	mpMp4mux = gst_element_factory_make("mp4mux", "mymux");
	mpFilesink = gst_element_factory_make("filesink", "myfileoutput");
    
	check_element_creation(mpPipeline, "pipeline");
	check_element_creation(mpAppsrc, "appsrc");
	check_element_creation(mpVideoconvert, "videoconvert");
	check_element_creation(mpX264enc, "x264encoder");
	check_element_creation(mpMp4mux, "mp4muxer");
	check_element_creation(mpFilesink, "filesink");

	if (!mpPipeline||!mpAppsrc||!mpVideoconvert||!mpX264enc||!mpMp4mux||!mpFilesink){
		RCLCPP_ERROR(mpLogger, "Not all elements could be created.");
		mpVideoLoggerStatus = -1;
	}
	else{
		mpVideoLoggerStatus = 1;
  		// Set the properties for filesink
		g_object_set(mpFilesink, "location", mpOutputFilePath.c_str(), NULL);
		// Build the pipeline
		gst_bin_add(GST_BIN(mpPipeline), mpAppsrc);
		gst_bin_add(GST_BIN(mpPipeline), mpVideoconvert);
		gst_bin_add(GST_BIN(mpPipeline), mpX264enc);
		gst_bin_add(GST_BIN(mpPipeline), mpMp4mux);
		gst_bin_add(GST_BIN(mpPipeline), mpFilesink);

		// Link the elements
		check_linking(gst_element_link(mpAppsrc, mpVideoconvert), "appsrc to videoconvert");
		check_linking(gst_element_link(mpVideoconvert, mpX264enc), "videoconvert to x264encoder");
		check_linking(gst_element_link(mpX264enc, mpMp4mux), "x264encoder to mp4 muxer");
		check_linking(gst_element_link(mpMp4mux, mpFilesink), "mp4 muxer to filesink");
		
		// Configure appsrc element
		std::string stringCaps = "video/x-raw, format=(string)BGR, width=(int)" + std::to_string(mpOutputImageWidth) + ", height=(int)" + std::to_string(mpOutputImageHeight) + ", framerate=(fraction)" + std::to_string(mpInputDataFPS) + "/1";
  		GstCaps* caps = gst_caps_from_string(stringCaps.c_str());
		g_object_set(mpAppsrc, "caps",  caps, nullptr);
		gst_caps_unref(caps);
		g_object_set(mpAppsrc, "format", GST_FORMAT_TIME, nullptr);
  		gst_element_set_state(mpPipeline, GST_STATE_PLAYING);
	}
}

VideoLogger::~VideoLogger(){
	gst_app_src_end_of_stream(GST_APP_SRC(mpAppsrc));

	GstBus *bus = gst_element_get_bus(mpPipeline);
	GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS ));
	if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR) {
		RCLCPP_ERROR(mpLogger, "An error occurred! Re-run with the GST_DEBUG=*:WARN environment variable set for more details.");
	}

	if (msg != NULL)
		gst_message_unref(msg);

	gst_object_unref(bus);
	gst_element_set_state(mpPipeline, GST_STATE_NULL);
	gst_object_unref(mpPipeline);
}

void VideoLogger::logFrame(cv::Mat &image){
	GstBuffer *buf;
	buf = gst_buffer_new_and_alloc(mpOutputFrameSize); 
	gst_buffer_map(buf, &mpMap, GST_MAP_WRITE);
	memcpy(mpMap.data, image.data, mpOutputFrameSize * image.elemSize());
	// TODO put custom timestamp here and test
	buf->pts = GST_MSECOND * 30 * mpFrameCounter;
	buf->dts = buf->pts;
	buf->duration = GST_MSECOND * 33;
	gst_buffer_unmap(buf, &mpMap);
	gst_app_src_push_buffer(GST_APP_SRC(mpAppsrc), buf);
	mpFrameCounter++;	
	cv::imshow("image", image);
	cv::waitKey(1);
}

VideoLoggerNode::VideoLoggerNode(std::string nodeName): Node(nodeName){
	// Parameter Declaration
	rcl_interfaces::msg::ParameterDescriptor outputFilePathDesc;
	outputFilePathDesc.description = "Path to output file for received video";
	outputFilePathDesc.type = 4;
	this->declare_parameter<std::string>("output_file_path", "/ws/data/output.mp4", outputFilePathDesc);
	rcl_interfaces::msg::ParameterDescriptor widthDesc;
	widthDesc.description = "Width of output image";
	widthDesc.type = 2;
	this->declare_parameter<int>("width", 640, widthDesc);
	rcl_interfaces::msg::ParameterDescriptor heightDesc;
	heightDesc.description = "Height of output image";
	heightDesc.type = 2;
	this->declare_parameter<int>("height", 480, heightDesc);
	rcl_interfaces::msg::ParameterDescriptor fpsDesc;
	fpsDesc.description = "FPS of input image stream";
	fpsDesc.type = 2;
	this->declare_parameter<int>("fps", 30, fpsDesc);
	rcl_interfaces::msg::ParameterDescriptor topicNameDesc;
	topicNameDesc.description = "Name of camera topic to subscribe to";
	topicNameDesc.type = 4;
	this->declare_parameter<std::string>("camera_topic", "/camera", topicNameDesc);
	rcl_interfaces::msg::ParameterDescriptor imageTypeDesc;
	imageTypeDesc.description = "Type of Image received by logger";
	imageTypeDesc.type = 4;
	this->declare_parameter<std::string>("image_type", "bgr", imageTypeDesc);

	mpOutputFilePath = this->get_parameter("output_file_path").as_string();
	mpOutputImageWidth = this->get_parameter("width").as_int();
	mpOutputImageHeight = this->get_parameter("height").as_int();
	mpInputDataFPS = this->get_parameter("fps").as_int();
	mpTopicName = this->get_parameter("camera_topic").as_string();
	mpImageType = this->get_parameter("image_type").as_string();
	RCLCPP_INFO(this->get_logger(), "Set Video Logger Parameters");
	if(mpImageType.compare("bgr") == 0){
		mpNumChannels = 3;	
	}

	VideoProperties props;
	props.mWidth = mpOutputImageWidth;
	props.mHeight = mpOutputImageHeight;
	props.mNumChannels = mpNumChannels;
	props.mFPS = mpInputDataFPS;
	mpVideoLogger = std::make_unique<VideoLogger>(this->get_logger(), mpOutputFilePath, props);

	// Creating ROS2 Subscription for receiving frames to be written
	mpFrameSubscriber = this->create_subscription<sensor_msgs::msg::Image>(
		mpTopicName.c_str(),
		10,
		std::bind(&VideoLoggerNode::frameCallback, this, std::placeholders::_1)
	);
	RCLCPP_INFO(this->get_logger(), "Created Video Logger Node");
}

void VideoLoggerNode::frameCallback(const sensor_msgs::msg::Image::ConstSharedPtr &img){
	unsigned long s = img->header.stamp.sec;
	unsigned long ns = img->header.stamp.nanosec;
	unsigned long timestamp = s * 1e9 + ns;
	cv_bridge::CvImagePtr cv_ptr_img = cv_bridge::toCvCopy(img, img->encoding);
	if(!cv_ptr_img->image.empty()){
		cv::Mat image;
		if(cv_ptr_img->image.rows == mpOutputImageHeight && cv_ptr_img->image.cols == mpOutputImageWidth){
			image = cv_ptr_img->image.clone();
		}
		else{
			cv::resize(cv_ptr_img->image, image, cv::Size(mpOutputImageWidth, mpOutputImageHeight));
		}
		mpVideoLogger->logFrame(image);
	}
	else{
		RCLCPP_ERROR(this->get_logger(), "Got empty image message");
	}
}
