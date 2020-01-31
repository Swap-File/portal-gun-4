#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <gst/gst.h>
#include "../common/gstcontext.h"
#include "../common/effects.h"

GstPipeline *pipeline[1];

static bool playing;

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
	gstcontext_load_pipeline(pipeline[0],GST_STATE_PAUSED,"rpicamsrc preview=0 ! image/jpeg,width=640,height=480,framerate=30/1 ! jpegparse ! jpegdec ! queue ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true");
	playing = false;
	//gstcontext_load_pipeline(0 ,"rpicamsrc preview=0 ! image/jpeg,width=1366,height=768,framerate=40/1 ! jpegparse ! jpegdec ! queue ! glupload ! glcolorconvert ! glcolorscale  ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	//8.25 top  9.5 right  good ratio
	
	//gstcontext_load_pipeline(0 ,"rpicamsrc preview=0 ! image/jpeg,width=640,height=480,framerate=40/1 ! jpegparse ! jpegdec ! queue ! glupload ! glcolorconvert ! glcolorscale  ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	//10.25 top  9.5 right  squished
	
	//gstcontext_load_pipeline(0 ,"rpicamsrc preview=0 ! image/jpeg,width=1640,height=922,framerate=30/1 ! jpegparse ! jpegdec ! queue ! glupload ! glcolorconvert ! glcolorscale  ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	//8.25 top  9.5 right  good ratio

	//gstcontext_load_pipeline(0 ,"rpicamsrc preview=0 ! image/jpeg,width=1280,height=720,framerate=40/1 ! jpegparse ! jpegdec ! queue ! glupload ! glcolorconvert ! glcolorscale  ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	//8.25 top  9.5 right  maybe a little wide?
	
	//gstcontext_load_pipeline(0 ,"rpicamsrc preview=0 ! image/jpeg,width=1640,height=1232,framerate=40/1 ! jpegparse ! jpegdec ! queue ! glupload ! glcolorconvert ! glcolorscale  ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	//10.25 top  9.5 right  squished
	
	//gstcontext_load_pipeline(0 ,"rpicamsrc preview=0 ! image/jpeg,width=640,height=480,framerate=30/1 ! jpegparse ! jpegdec ! queue ! glupload ! glcolorconvert ! glcolorscale  ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	//8.25 top  9.5 right  good ratio
	
	//gstcontext_load_pipeline(pipeline[0] ,"videotestsrc pattern=18 ! video/x-raw,width=640,height=480,framerate=(fraction)30/1 ! queue ! glupload ! glcolorconvert  ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true");
}