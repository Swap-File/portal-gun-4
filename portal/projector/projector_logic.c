#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <gst/gst.h>
#include "../common/gstcontext.h"

GstPipeline *pipeline[1];


void logic_init(void){

	gstcontext_load_pipeline(pipeline[0],GST_STATE_PLAYING,"videotestsrc pattern=24 ! queue ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true");
}