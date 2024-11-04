#include "sensors/video/reader.h"

VideoReader::VideoReader(rclcpp::Logger logger, const std::string& inputFilePath, std::function<void(std::shared_ptr<Frame>)> cb)
    : mpLogger(logger), inputFilePath(inputFilePath), mpFrameCallback(cb), mpPipeline(nullptr),
      mpFilesrc(nullptr), mpDemuxer(nullptr), mpQueue(nullptr), mpH264Parser(nullptr),
      mpH264Decoder(nullptr), mpVideoconvert(nullptr), mpCapsfilter(nullptr), mpAppsink(nullptr),
      loop(nullptr) {
    
    gst_init(nullptr, nullptr);

    // Create elements
    mpPipeline = gst_pipeline_new("video-reader-pipeline");
    mpFilesrc = gst_element_factory_make("filesrc", "source");
    mpDemuxer = gst_element_factory_make("qtdemux", "demuxer");
    mpQueue = gst_element_factory_make("queue", "queue");
    mpH264Parser = gst_element_factory_make("h264parse", "parser");
    mpH264Decoder = gst_element_factory_make("avdec_h264", "decoder");
    mpVideoconvert = gst_element_factory_make("videoconvert", "converter");
    mpCapsfilter = gst_element_factory_make("capsfilter", "capsfilter");
    mpAppsink = gst_element_factory_make("appsink", "sink");

    check_element_creation(mpPipeline, "pipeline");
    check_element_creation(mpFilesrc, "filesrc");
    check_element_creation(mpDemuxer, "qtdemux");
    check_element_creation(mpQueue, "queue");
    check_element_creation(mpH264Parser, "h264parse");
    check_element_creation(mpH264Decoder, "avdec_h264");
    check_element_creation(mpVideoconvert, "videoconvert");
    check_element_creation(mpCapsfilter, "capsfilter");
    check_element_creation(mpAppsink, "appsink");

    // Set properties
    g_object_set(mpFilesrc, "location", inputFilePath.c_str(), NULL);
    g_object_set(mpAppsink, "emit-signals", TRUE, "sync", FALSE, NULL);

    // Create a caps for BGR format
    GstCaps *caps = gst_caps_new_simple("video/x-raw",
        "format", G_TYPE_STRING, "BGR",
        NULL);
    g_object_set(mpCapsfilter, "caps", caps, NULL);
    gst_caps_unref(caps);  // Unref the caps after use

    // Build the pipeline
    gst_bin_add_many(GST_BIN(mpPipeline), mpFilesrc, mpDemuxer, mpQueue, mpH264Parser, mpH264Decoder, mpVideoconvert, mpCapsfilter, mpAppsink, NULL);

    // Link elements with error handling
    check_linking(gst_element_link(mpFilesrc, mpDemuxer), "source to demuxer");
    check_linking(gst_element_link_many(mpQueue, mpH264Parser, mpH264Decoder, mpVideoconvert, mpCapsfilter, mpAppsink, NULL), "linking elements");

    // Connect to the pad-added signal
    g_signal_connect(mpDemuxer, "pad-added", G_CALLBACK(+[](GstElement *src, GstPad *pad, gpointer data) {
        GstElement *mpQueue = (GstElement *)data;
        GstPad *sink_pad = gst_element_get_static_pad(mpQueue, "sink");
        if (gst_pad_link(pad, sink_pad) != GST_PAD_LINK_OK) {
            g_print("Failed to link demuxer pad to queue.\n");
        }
        gst_object_unref(sink_pad);
    }), mpQueue);
    
    // Create a cv::VideoWriter to save the output
    // writer.open(outputFilePath, cv::VideoWriter::fourcc('M', 'P', '4', 'V'), 30, cv::Size(1920, 1080), true);
    // if (!writer.isOpened()) {
    //     std::cerr << "Could not open the output video file." << std::endl;
    // } else {
    //     std::cout << "Output video file opened successfully." << std::endl;
    // }

    // Connect to appsink's new-sample signal
    g_signal_connect(mpAppsink, "new-sample", G_CALLBACK(new_sample), this);
}

VideoReader::~VideoReader() {
    // Clean up
    if (loop) {
        g_main_loop_unref(loop);
    }
    if (mpPipeline) {
        gst_element_set_state(mpPipeline, GST_STATE_NULL);
        gst_object_unref(mpPipeline);
    }
    // writer.release(); // Release the VideoWriter
}

void VideoReader::frameCallback(std::shared_ptr<Frame> frame){
	this->mpFrameCallback(frame);
}

GstFlowReturn VideoReader::new_sample(GstAppSink *sink, gpointer user_data) {
    VideoReader *reader = static_cast<VideoReader*>(user_data);

    // Retrieve the sample from appsink
    GstSample *sample = gst_app_sink_pull_sample(sink);
    if (!sample) {
        return GST_FLOW_ERROR; // Error if no sample is available
    }

    // Get the buffer from the sample
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    if (buffer) {
        // Map the buffer to read its data
        GstMapInfo map;
		if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
			// Create a cv::Mat from the buffer data
			GstClockTime timestamp = GST_BUFFER_PTS(buffer);
			double timestamp_seconds = (double)timestamp / GST_SECOND; // Convert to seconds

			// Log the timestamp
			// std::cout << "Processing frame at timestamp: " << timestamp_seconds << " seconds" << std::endl;
			cv::Mat frame(cv::Size(1920, 1080), CV_8UC3, (void*)map.data, cv::Mat::AUTO_STEP);

			std::shared_ptr<Frame> f = std::make_shared<Frame>();
			f->frame = frame;
			f->timestamp = timestamp_seconds;
			reader->frameCallback(f);
            // Write the frame to the video file
            // if (reader->writer.isOpened()) {
            //     reader->writer.write(frame); // Write the frame
            // }
            //
            gst_buffer_unmap(buffer, &map); // Unmap the buffer after processing
        }
    }

    gst_sample_unref(sample); // Unref the sample after processing
    return GST_FLOW_OK;
}

gboolean VideoReader::bus_call(GstBus *bus, GstMessage *msg) {
    switch (msg->type) {
        case GST_MESSAGE_EOS:
            g_print("End of stream\n");
            g_main_loop_quit(loop);
            break;
        case GST_MESSAGE_ERROR: {
            GError *err;
            gchar *debug_info;
            gst_message_parse_error(msg, &err, &debug_info);
            std::cerr << "Error: " << err->message << std::endl;
            std::cerr << "Debug Info: " << (debug_info ? debug_info : "none") << std::endl;
            g_clear_error(&err);
            g_free(debug_info);
            g_main_loop_quit(loop);
            break;
        }
        default:
            break;
    }
    return G_SOURCE_CONTINUE;
}

void VideoReader::run() {
    loop = g_main_loop_new(nullptr, FALSE);
    
    // Get the bus and add a watch
    GstBus *bus = gst_element_get_bus(mpPipeline);
    gst_bus_add_watch(bus, +[](GstBus *bus, GstMessage *msg, gpointer data) {
        return static_cast<VideoReader*>(data)->bus_call(bus, msg);
    }, this);

    // Start the pipeline
    check_linking(gst_element_set_state(mpPipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE, "pipeline state to PLAYING");

    // Run the main loop
    // RCLCPP_INFO(this->get_logger(), "Running the pipeline...");
	std::cout<<"Running the pipeline..."<<std::endl;
    g_main_loop_run(loop);

    gst_object_unref(bus); // Unref the bus after use
	gst_element_set_state(mpPipeline, GST_STATE_NULL);
	gst_object_unref(mpPipeline);
	g_main_loop_unref(loop);
}


VideoReaderNode::VideoReaderNode() : rclcpp::Node("video_reader"){
	rcl_interfaces::msg::ParameterDescriptor topicNameDesc;
	topicNameDesc.description = "Name of camera topic to subscribe to";
	topicNameDesc.type = 4;
	this->declare_parameter<std::string>("camera_topic", "/camera", topicNameDesc);
	rcl_interfaces::msg::ParameterDescriptor inputFilePathDesc;
	inputFilePathDesc.description = "Path to input file for received video";
	inputFilePathDesc.type = 4;
	this->declare_parameter<std::string>("input_file_path", "/ws/data/src.mp4", inputFilePathDesc);
	rcl_interfaces::msg::ParameterDescriptor widthDesc;
	widthDesc.description = "Width of output image";
	widthDesc.type = 2;
	// this->declare_parameter<int>("width", 640, widthDesc);
	// rcl_interfaces::msg::ParameterDescriptor heightDesc;
	// heightDesc.description = "Height of output image";
	// heightDesc.type = 2;
	// this->declare_parameter<int>("height", 480, heightDesc);
	// rcl_interfaces::msg::ParameterDescriptor fpsDesc;
	// fpsDesc.description = "FPS of input image stream";
	// fpsDesc.type = 2;
	// this->declare_parameter<int>("fps", 30, fpsDesc);

	mpInputFilePath = this->get_parameter("input_file_path").as_string();
	mpCameraTopic = this->get_parameter("camera_topic").as_string();
	RCLCPP_INFO(this->get_logger(), "Reading file from %s", mpInputFilePath.c_str());
	RCLCPP_INFO(this->get_logger(), "Topic for publication: %s", mpCameraTopic.c_str());
	RCLCPP_INFO(this->get_logger(), "Set Video Reader Parameters");
	
	mpFramePub = this->create_publisher<sensor_msgs::msg::Image>(mpCameraTopic, 10);	

	std::function<void(std::shared_ptr<Frame>)> fcb = [this](std::shared_ptr<Frame> frame){
		frameCallback(frame);
	};
	mpReader = std::make_shared<VideoReader>(this->get_logger(), mpInputFilePath, fcb);	
	mpReader->run();
	RCLCPP_INFO(this->get_logger(), "Test complete");
}

void VideoReaderNode::frameCallback(std::shared_ptr<Frame> frame){
	RCLCPP_DEBUG(this->get_logger(), "Got frame with timestamp: %f", frame->timestamp);
	if(!frame->frame.empty()){
		sensor_msgs::msg::Image img_msg;
		cv_bridge::CvImage(
				std_msgs::msg::Header(),
				sensor_msgs::image_encodings::RGB8,
				frame->frame).toImageMsg(img_msg);
		// img_msg.header.stamp = t;
		mpFramePub->publish(img_msg);
	}
}
