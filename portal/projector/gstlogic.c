#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <gst/gst.h>
#include "../common/gstcontext.h"

GstPipeline *pipeline[1];

void cam_state(bool state){
	static bool current_state = false;
	if (current_state != state){
		if (state == true){
			gst_element_set_state (GST_ELEMENT (pipeline[0]), GST_STATE_PLAYING);	
		}else {
			gst_element_set_state (GST_ELEMENT (pipeline[0]), GST_STATE_PAUSED);
		}
	current_state = state;
	}
}
void cam_init(void){

	gstshared_init();
	load_pipeline(0 ,"rpicamsrc preview=0 ! image/jpeg,width=640,height=480,framerate=30/1 ! jpegparse ! jpegdec ! queue ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true");

}