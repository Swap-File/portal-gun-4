#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <gst/gst.h>
#include "../common/gstcontext.h"
#include "../common/effects.h"

GstPipeline *pipeline;

static bool playing;

void console_logic(int gst_state) {

    if (gst_state == GST_RPICAMSRC && playing == false) {
        printf("Starting GST_RPICAMSRC\n");
        gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
        playing = true;
    }

    if (gst_state != GST_RPICAMSRC && playing == true) {
        printf("Stopping GST_RPICAMSRC\n");
        gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_NULL);
        playing = false;
    }
}

void console_logic_init(bool gordon) {

    if(gordon) {
        gstcontext_load_pipeline(GST_RPICAMSRC,&pipeline,GST_STATE_NULL,"rpicamsrc preview=0 awb-mode=10 ! video/x-raw,width=640,height=480,framerate=30/1 ! "
                                 "queue ! tee name=t "
                                 "t. ! queue ! jpegenc ! rtpjpegpay ! udpsink host=192.168.3.21 port=9000 sync=false "
                                 "t. ! queue ! videorate ! video/x-raw,framerate=10/1 ! videoscale ! video/x-raw,width=400,height=240 ! videoflip method=3 ! jpegenc ! multifilesink location=/var/www/html/tmp/snapshot.jpg sync=false "
                                 "t. ! queue ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false"
                                );  //the order of this matters, the fakesink MUST Be last in the tee chain!
    } else {
        gstcontext_load_pipeline(GST_RPICAMSRC,&pipeline,GST_STATE_NULL,"rpicamsrc preview=0 awb-mode=10 ! video/x-raw,width=640,height=480,framerate=30/1 ! "
                                 "queue ! tee name=t "
                                 "t. ! queue ! jpegenc ! rtpjpegpay ! udpsink host=192.168.3.20 port=9000 sync=false "
                                 "t. ! queue ! videorate ! video/x-raw,framerate=10/1 ! videoscale ! video/x-raw,width=400,height=240 ! videoflip method=3 ! jpegenc ! multifilesink location=/var/www/html/tmp/snapshot.jpg sync=false "
                                 "t. ! queue ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false"
                                );  //the order of this matters, the fakesink MUST Be last in the tee chain!
    }
	
  playing = false;
 
}
