#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <gst/gst.h>
#include "../common/gstcontext.h"

GstPipeline *pipeline[1];


void gstlogic_init(void){
	//gstcontext_load_pipeline(0 ,"rpicamsrc preview=0 ! image/jpeg,width=640,height=480,framerate=30/1 ! jpegparse ! jpegdec ! queue ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true");
	gstcontext_load_pipeline(pipeline[0] ,"videotestsrc ! queue ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true");
}