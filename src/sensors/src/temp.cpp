#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <opencv2/opencv.hpp>

#include <cassert>

int main() {
	GstElement *pipeline, *appsrc, *videoconvert, *x264enc, *mp4mux, *filesink,
			   *autovideosink;
	GstCaps *caps;
	GstMapInfo map;

	gst_init(nullptr, nullptr);

	pipeline = gst_pipeline_new("mypipeline");

	// Create elements
	appsrc = gst_element_factory_make("appsrc", "mysource");
	videoconvert = gst_element_factory_make("videoconvert", "myconvert");
	x264enc = gst_element_factory_make("x264enc", "myencoder");
	mp4mux = gst_element_factory_make("mp4mux", "mymux");
	filesink = gst_element_factory_make("filesink", "myfileoutput");

	if (!pipeline || !appsrc || !videoconvert || !x264enc || !mp4mux ||
			!filesink) {
		g_printerr("Not all elements could be created.\n");
		// return -1;
	}

	// Set the properties for filesink
	g_object_set(filesink, "location", "/ws/output.mp4", NULL);

	// Build the pipeline
	gst_bin_add(GST_BIN(pipeline), appsrc);
	gst_bin_add(GST_BIN(pipeline), videoconvert);
	gst_bin_add(GST_BIN(pipeline), x264enc);
	gst_bin_add(GST_BIN(pipeline), mp4mux);
	gst_bin_add(GST_BIN(pipeline), filesink);

	// Link the elements
	gst_element_link(appsrc, videoconvert);
	gst_element_link(videoconvert, x264enc);
	gst_element_link(x264enc, mp4mux);
	gst_element_link(mp4mux, filesink);

	caps =
		gst_caps_from_string("video/x-raw, format=(string)BGR, width=(int)1920, "
				"height=(int)1080, framerate=(fraction)30/1");

	g_object_set(appsrc, "caps",  caps, nullptr);
	gst_caps_unref(caps);
	g_object_set(appsrc, "format", GST_FORMAT_TIME, nullptr);

	gst_element_set_state(pipeline, GST_STATE_PLAYING);
	std::cout<<"created pipeline" <<std::endl;

	cv::VideoCapture cap;
	cap.open("/ws/src.mp4");

	if(!cap.isOpened())
		return -2;
	cv::Mat frame;
	int i = 0;
	GstBuffer *buf;
	while(true){
		cap.read(frame);
		if(frame.empty()){
			break;
		}
		buf = gst_buffer_new_and_alloc(1920 * 1080 * 3); 
		gst_buffer_map(buf, &map, GST_MAP_WRITE);
		memcpy(map.data, frame.data, frame.total() * frame.elemSize());
		// memcpy(buf, frame.data, 19)
		// buf = gst_buffer_new_wrapped(frame.data, frame.size().width * frame.size().height * frame.channels());

		buf->pts = GST_MSECOND * 30 * i;
		buf->dts = buf->pts;
		buf->duration = GST_MSECOND * 33;
		gst_buffer_unmap(buf, &map);
		gst_app_src_push_buffer(GST_APP_SRC(appsrc), buf);
		i++;
	}

	gst_app_src_end_of_stream(GST_APP_SRC(appsrc));

	GstBus *bus = gst_element_get_bus(pipeline);
	GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS ));
	if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR) {
		g_error ("An error occurred! Re-run with the GST_DEBUG=*:WARN environment "
				"variable set for more details.");
	}

	if (msg != NULL)
		gst_message_unref(msg);

	gst_object_unref(bus);
	gst_element_set_state(pipeline, GST_STATE_NULL);
	gst_object_unref(pipeline);
}
