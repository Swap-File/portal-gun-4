#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <gst/gst.h>
#include "../common/gstcontext.h"
#include "../common/effects.h"
#include "../common/memory.h"

//outgoing flag
static volatile bool *video_done_flag = NULL;  //points to shared struct (if set)

GstPipeline *pipeline[GST_LAST + 1],*pipeline_active;
GstPad *outputpads[6];
GstElement *outputselector;
	
static bool movie_is_playing = false;
static int video_mode_requested = 0;
static int video_mode_current = -1;

static void seek_to_time(gint64 time_nanoseconds)
{
	if (!gst_element_seek ( GST_ELEMENT(pipeline[GST_MOVIE_FIRST]), 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, time_nanoseconds, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
		g_print("Seek failed!\n");
	}
}

static void start_pipeline(void)
{	
	uint32_t start_time = millis();
	
	//stop the old pipeline
	if (GST_IS_ELEMENT(pipeline_active)) {
		
		//leaving a audio effect mode 
		if (video_mode_current >= GST_LIBVISUAL_FIRST && video_mode_current <= GST_LIBVISUAL_LAST){
			//always NULL alsa because libvisual doesnt stop emitting properly if paused
			gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_NULL);
			printf("Projector: Stopping alsa!\n");		
		}		
		//leaving a movie mode 
		else if (video_mode_current >= GST_MOVIE_FIRST && video_mode_current <= GST_MOVIE_LAST){
			if (video_mode_requested < GST_MOVIE_FIRST || video_mode_requested > GST_MOVIE_LAST){
				gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PAUSED);
				printf("Projector: Stopping video!\n");			
			}else{
				printf("Projector: Video hot swap!\n");	
			}	
			movie_is_playing = false;
		} else {//null everything else	
			printf("Projector: Null-ing Pipeline!\n");	
			gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_NULL);
		}
	}
	
	//prep the new pipeline
	if (video_mode_requested >= GST_MOVIE_FIRST && video_mode_requested <= GST_MOVIE_LAST){
		pipeline_active = pipeline[GST_MOVIE_FIRST]; //all share the same pipeline
		//find file pos and go there
		int index = video_mode_requested - GST_MOVIE_FIRST;
		seek_to_time(movie_start_times[index]);  
		movie_is_playing = true;
		
	}
	else if (video_mode_requested >= GST_LIBVISUAL_FIRST && video_mode_requested <= GST_LIBVISUAL_LAST){
		g_object_set (outputselector, "active-pad",  outputpads[video_mode_requested - GST_LIBVISUAL_FIRST], NULL);
		pipeline_active = pipeline[GST_LIBVISUAL_FIRST]; 
	} else {
		pipeline_active = pipeline[video_mode_requested]; 
	}
	
	//if we dont se the callbacks here, the bus request handler can do it
	//but explicitly setting it seems to be required when swapping pipelines
	gstcontext_set(&pipeline_active); 
	
	//start the show
	gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PLAYING);	

	video_mode_current = video_mode_requested;
	
	printf("Pipeline changed to %d - %s in %d milliseconds!\n",video_mode_current,effectnames[video_mode_current],millis() - start_time);
}

static gint64 get_position(void)
{
	gint64 pos;
	bool result = gst_element_query_position(GST_ELEMENT(pipeline[GST_MOVIE_FIRST]), GST_FORMAT_TIME, &pos);
	if (!result) pos = -1;
	return pos;
}

void projector_logic_update(int gst_state, int portal_state){

	video_mode_requested = gst_state;
		
	if (movie_is_playing){
		if (video_mode_current >= GST_MOVIE_FIRST && video_mode_current <= GST_MOVIE_LAST ){
			int index = video_mode_current - GST_MOVIE_FIRST;
			if (get_position() > movie_end_times[index]){
				printf("End of Chapter!\n");
				if (video_done_flag != NULL) *video_done_flag = true;
				gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PAUSED);
				movie_is_playing = false;
			}
			if (portal_state != PORTAL_OPEN_ORANGE && portal_state != PORTAL_OPEN_BLUE ) {
				printf("End EARLY!\n");
				if (video_done_flag != NULL) *video_done_flag = true;
				gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PAUSED);
				movie_is_playing = false;
			}
		}
	}

	if (video_mode_requested != video_mode_current)	{
		if (video_mode_requested >= GST_MOVIE_FIRST && video_mode_requested <= GST_MOVIE_LAST){
			if (portal_state == PORTAL_OPEN_ORANGE || portal_state == PORTAL_OPEN_BLUE) { //wait until portal is opening to start video
				start_pipeline();
			}
		} else { //immediately change for all other pipelines to allow as much preloading as possible
			start_pipeline();
		}
	}
}

void projector_logic_init(volatile bool *video_done_flag_p){

	video_done_flag = video_done_flag_p;

	//gstcontext_load_pipeline(pipeline[0],GST_STATE_PLAYING,"videotestsrc pattern=24 ! queue ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true");
	
	gstcontext_load_pipeline(GST_BLANK,				&pipeline[GST_BLANK],				GST_STATE_NULL,"videotestsrc pattern=2 ! video/x-raw,width=640,height=480,framerate=(fraction)30/1 ! queue ! glupload ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true");
	gstcontext_load_pipeline(GST_VIDEOTESTSRC,		&pipeline[GST_VIDEOTESTSRC],		GST_STATE_NULL,"videotestsrc ! video/x-raw,width=640,height=480,framerate=(fraction)30/1 ! queue ! glupload ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true");
	gstcontext_load_pipeline(GST_VIDEOTESTSRC,		&pipeline[GST_RPICAMSRC],			GST_STATE_NULL,"videotestsrc ! video/x-raw,width=640,height=480,framerate=(fraction)30/1 ! queue ! glupload ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true");
	gstcontext_load_pipeline(GST_VIDEOTESTSRC_CUBED,&pipeline[GST_VIDEOTESTSRC_CUBED],	GST_STATE_NULL,"videotestsrc ! video/x-raw,width=640,height=480,framerate=(fraction)30/1 ! queue ! glupload ! glfiltercube ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true");

	//normal
	gstcontext_load_pipeline(GST_NORMAL,&pipeline[GST_NORMAL],GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	
	//cpu effects drop framerate to 20 from 30 due to cpu usage
	gstcontext_load_pipeline(GST_MARBLE,		&pipeline[GST_MARBLE]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert ! queue ! marble        ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_KALEDIOSCOPE,	&pipeline[GST_KALEDIOSCOPE]	,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert ! queue ! kaleidoscope  ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_RADIOACTV,		&pipeline[GST_RADIOACTV]	,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert ! queue ! radioactv     ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_REVTV,			&pipeline[GST_REVTV]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert ! queue ! revtv         ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_AGINGTV,		&pipeline[GST_AGINGTV]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert ! queue ! agingtv       ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_DICETV,		&pipeline[GST_DICETV]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert ! queue ! dicetv        ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_WARPTV,		&pipeline[GST_WARPTV]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert ! queue ! warptv        ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_SHAGADELICTV,	&pipeline[GST_SHAGADELICTV]	,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert ! queue ! shagadelictv  ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_VERTIGOTV,		&pipeline[GST_VERTIGOTV]	,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert ! queue ! vertigotv     ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_RIPPLETV,		&pipeline[GST_RIPPLETV]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert ! queue ! rippletv      ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_EDGETV,		&pipeline[GST_EDGETV]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert ! queue ! edgetv        ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_STREAKTV,		&pipeline[GST_STREAKTV]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert ! queue ! streaktv      ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_AATV,			&pipeline[GST_AATV]			,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert                                                                                       ! queue ! aatv rain-mode=0 ! glupload ! glcolorconvert ! glcolorscale ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_CACATV,		&pipeline[GST_CACATV]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! videoscale ! videoconvert ! video/x-raw,width=160,height=120,framerate=30/1,format=RGB ! queue ! cacatv           ! glupload ! glcolorconvert ! glcolorscale ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");

	//gl effects 
	gstcontext_load_pipeline(GST_GLCUBE,		&pipeline[GST_GLCUBE]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! glfiltercube      ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_GLMIRROR,		&pipeline[GST_GLMIRROR]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_mirror  ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_GLSQUEEZE,		&pipeline[GST_GLSQUEEZE]	,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_squeeze ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_GLSTRETCH,		&pipeline[GST_GLSTRETCH]	,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_stretch ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_GLTUNNEL,		&pipeline[GST_GLTUNNEL]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_tunnel  ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_GLTWIRL,		&pipeline[GST_GLTWIRL]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_twirl   ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_GLBULGE,		&pipeline[GST_GLBULGE]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_bulge   ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_GLHEAT,		&pipeline[GST_GLHEAT]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_heat    ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");

	//audio effects - alsasrc takes a second or so to init, so here a output-selector is used
	//effect order matters since pads on the output selector can't easily be named in advance 
	//audio format must match the movie output stuff, otherwise the I2S Soundcard will get slow and laggy when switching formats!
	gstcontext_load_pipeline(GST_LIBVISUAL_FIRST,&pipeline[GST_LIBVISUAL_FIRST],GST_STATE_PAUSED,"alsasrc buffer-time=40000 ! audio/x-raw,layout=interleaved,rate=48000,format=S32LE,channels=2 ! queue max-size-time=10000000 leaky=downstream ! audioconvert ! "
	"output-selector name=audioin pad-negotiation-mode=Active "
	"audioin. ! libvisual_jess     ! videosink. "
	"audioin. ! libvisual_infinite ! videosink. "
	"audioin. ! libvisual_jakdaw   ! videosink. "
	"audioin. ! libvisual_oinksie  ! videosink. "
	"audioin. ! goom               ! videosink. "
	"audioin. ! goom2k1            ! videosink. "
	"funnel name=videosink ! video/x-raw,width=400,height=320,framerate=30/1 ! "
	"glupload ! glcolorconvert ! glcolorscale ! video/x-raw(memory:GLMemory),width=640,height=480 ! "
	"glfilterapp name=grabtexture ! fakesink sync=true async=false");
	
	//movie pipeline, has all videos as long long video, chapter start and end times stored in gstvideo.h
	//it doesnt work well to load and unload various input files due to the 
	//audio format must match the visual input stuff, otherwise the I2S Soundcard will get slow and laggy when switching formats! 
	gstcontext_load_pipeline(GST_MOVIE_FIRST,&pipeline[GST_MOVIE_FIRST],GST_STATE_PAUSED,"filesrc location=/home/pi/assets/movies/all.mp4 ! qtdemux name=dmux "
	"dmux.video_0 ! queue ! avdec_h264 ! queue ! videoconvert ! "
	"glupload ! glcolorscale ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true async=false "
	"dmux.audio_0 ! queue ! aacparse ! avdec_aac ! audioconvert ! audio/x-raw,layout=interleaved,rate=48000,format=S32LE,channels=2 ! alsasink sync=true async=false device=dmix");
	//"dmux.audio_0 ! fakesink");  //disable sound for testing on the workbench

	//save the output pads from the visualization pipelines
	//get the output-selector element
	outputselector = gst_bin_get_by_name (GST_BIN (pipeline[GST_LIBVISUAL_FIRST]), "audioin");
	
	//save each output pad for later
	outputpads[0] = gst_element_get_static_pad(outputselector,"src_0");
	outputpads[1] = gst_element_get_static_pad(outputselector,"src_1");
	outputpads[2] = gst_element_get_static_pad(outputselector,"src_2");
	outputpads[3] = gst_element_get_static_pad(outputselector,"src_3");
	outputpads[4] = gst_element_get_static_pad(outputselector,"src_4");
	outputpads[5] = gst_element_get_static_pad(outputselector,"src_5");

}