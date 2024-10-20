#include <gst/gst.h>
#include <glib.h>
#include <gst/app/gstappsrc.h>
#include <opencv2/opencv.hpp>
#include <unistd.h>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;


typedef struct {
    GstPipeline *pipeline = nullptr;
    GstAppSrc  *app_src = nullptr;
    GstElement *video_convert = nullptr;
    GstElement *encoder = nullptr;
    GstElement *h264_parser = nullptr;
    GstElement *qt_mux = nullptr;
    GstElement *file_sink = nullptr;
}CustomData;



int main(int argc,char * argv[]){
    CustomData data;
    GstBus *bus = nullptr;
    GstMessage *msg = nullptr;
    GstStateChangeReturn ret;
    gboolean terminate = false;
    GstClockTime timestamp = 0;
    gst_init(&argc, &argv); 

    data.pipeline = (GstPipeline*)gst_pipeline_new("m_pipeline");
    data.app_src = (GstAppSrc*)gst_element_factory_make("appsrc","m_app_src");
    data.video_convert = gst_element_factory_make("videoconvert","m_video_convert");
    data.encoder = gst_element_factory_make("x264enc","m_x264enc");
    data.h264_parser = gst_element_factory_make("h264parse","m_h264_parser");
    data.qt_mux = gst_element_factory_make("qtmux","qt_mux");
    data.file_sink = gst_element_factory_make("filesink","file_sink");

	if(!data.app_src){
        g_printerr("failed to create app src element\n");
		return -1;
	}
	if(!data.video_convert){
        g_printerr("failed to create video convert element\n");
		return -1;
	}
	if(!data.encoder){
        g_printerr("failed to create encoder element\n");
		return -1;
	}
	if(!data.h264_parser){
        g_printerr("failed to create encr element\n");
		return -1;
	}
	if(!data.qt_mux){
        g_printerr("failed to create qt_mux element\n");
		return -1;
	}
	if(!data.file_sink){
        g_printerr("failed to create file_sink element\n");
		return -1;
	}
	if(!data.pipeline){
        g_printerr("failed to create pipeline\n");
		return -1;
	}

    // if (!data.app_src || !data.video_convert || !data.encoder || !data.h264_parser || !data.qt_mux || !data.file_sink || !data.pipeline){
    //     g_printerr("failed to create all elements\n");
    //     return -1;
    // }

    gst_bin_add_many(GST_BIN(data.pipeline), (GstElement*)data.app_src, data.video_convert, data.encoder, data.h264_parser, data.qt_mux, data.file_sink, NULL);

    g_assert(gst_element_link_many((GstElement*)data.app_src, data.video_convert, data.encoder, data.h264_parser, data.qt_mux, data.file_sink,NULL));

    GstCaps *caps = gst_caps_new_simple("video/x-raw","format",G_TYPE_STRING,"BGR",
                                        "width",G_TYPE_INT,1920,
                                        "height",G_TYPE_INT,1080,
                                        "framerate",GST_TYPE_FRACTION,30,1,
                                        NULL);

    gst_app_src_set_caps(GST_APP_SRC(data.app_src), caps);
    g_object_set(data.app_src,"is_live",true,NULL);
    g_object_set(data.app_src,"format",GST_FORMAT_TIME,NULL);

    std::string mp4_url = "/ws/des.mp4";

    g_object_set(data.file_sink,"location",mp4_url.c_str(),NULL);

    ret = gst_element_set_state((GstElement*)data.pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE){
        g_printerr("Unable to set the pipeline to the playing state. \n");
        gst_object_unref(data.pipeline);
        return -1;
    }

    cv::VideoCapture cap;
    cap.open("/ws/src.mp4");
    
    if(!cap.isOpened())
        return -2;
    cv::Mat frame;
    while(true){
        cap.read(frame);
        if(frame.empty()){
            break;
        }
        GstBuffer *buffer;
        buffer = gst_buffer_new_wrapped(frame.data, frame.size().width * frame.size().height * frame.channels());

        GST_BUFFER_PTS (buffer) = timestamp;
        GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale_int (1, GST_SECOND, 25);
		timestamp += GST_BUFFER_DURATION (buffer);
        GstFlowReturn ret;

        g_signal_emit_by_name(data.app_src, "push-buffer", buffer, &ret);
        usleep(1000000/25);
    }
	gst_app_src_end_of_stream(data.app_src);
	int i = 0;
	while(i < 10000000){
		i++;
	}

    gst_element_set_state((GstElement*)data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);
    return 0;
}

