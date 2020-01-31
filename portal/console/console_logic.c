#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <gst/gst.h>
#include "../common/gstcontext.h"
#include "../projector/projector.h"

GstPipeline *pipeline[1];

static bool playing = false;

void console_logic(int gst_state){

	if (gst_state == GST_RPICAMSRC && playing == false){
		gst_element_set_state (GST_ELEMENT (pipeline[0]), GST_STATE_PLAYING);
		playing = true;
	}
	
	if (gst_state != GST_RPICAMSRC && playing == true){
		gst_element_set_state (GST_ELEMENT (pipeline[0]), GST_STATE_PAUSED);
		playing = false;
	}
}


void console_logic_init(void){
	gstcontext_load_pipeline(pipeline[0] ,"rpicamsrc preview=0 ! image/jpeg,width=640,height=480,framerate=30/1 ! jpegparse ! jpegdec ! queue ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true");
	//gstcontext_load_pipeline(pipeline[0] ,"videotestsrc pattern=18 ! queue ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true");
}