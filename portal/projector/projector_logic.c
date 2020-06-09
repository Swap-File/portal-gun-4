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
	
static bool movie_is_playing = false;
static bool movie_is_loaded = false;
static bool movie_is_primed = false;
static int video_mode_requested = 0;
static int video_mode_current = -1;
uint32_t movie_start_time = 0;

static void seek_to_time(gint64 time_nanoseconds,GstPipeline **pipeline)
{
	if (!gst_element_seek ( GST_ELEMENT(*pipeline), 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, time_nanoseconds, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
		g_print("Seek failed!\n");
	}
}

gint64 get_position(GstPipeline **pipeline)
{
	gint64 pos;
	bool result = gst_element_query_position ( GST_ELEMENT(*pipeline), GST_FORMAT_TIME, &pos);
	if (!result) pos = -1;
	return pos;
}

void mute_pipeline( bool state, GstPipeline **pipeline){
	GstElement *outputselector = gst_bin_get_by_name (GST_BIN (*pipeline), "vol");
	g_object_set (outputselector, "mute",  state, NULL);
}

static void start_pipeline(void)
{
	uint32_t start_time = millis();
	
	//stop the old pipeline
	
	if (GST_IS_ELEMENT(pipeline_active)) {
		if (video_mode_current >= GST_MOVIE_FIRST && video_mode_current <= GST_MOVIE_LAST){
			printf("Projector: Pause & Rewind Video Pipeline!\n");
			seek_to_time(0,&pipeline_active); //please be kind rewind
			gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PAUSED);
		}else{
			gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_NULL);
			printf("Projector: Null-ing Pipeline!\n");
		}
	}
	
	pipeline_active = pipeline[video_mode_requested]; 
	
	//if we dont se the callbacks here, the bus request handler can do it
	//but explicitly setting it seems to be required when swapping pipelines
	gstcontext_set(&pipeline_active); 
	
	//start the show
	if (video_mode_requested >= GST_MOVIE_FIRST && video_mode_requested <= GST_MOVIE_LAST){
		mute_pipeline(true, &pipeline_active);
		movie_start_time = millis();
		printf("GST_STATE_PAUSED!\n");
		movie_is_loaded = true;
		movie_is_primed = false;
	}
	
	gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PLAYING);
	
	video_mode_current = video_mode_requested;

	printf("Pipeline changed to %d - %s in %d milliseconds!\n",video_mode_current,effectnames[video_mode_current],millis() - start_time);
}

void projector_logic_update(int gst_state, int portal_state){

	video_mode_requested = gst_state;
	
	//if a movie is loaded and the portal is open, start it playing
	if (movie_is_loaded){
		if ((portal_state & PORTAL_OPEN_BIT) == PORTAL_OPEN_BIT) { //wait until portal is opening to start video
				gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PLAYING);
				movie_is_loaded = false;
				movie_is_playing = true;
				//if we didnt get primed in time, unmute it now
				if (!movie_is_primed) mute_pipeline(false, &pipeline_active);
		}
	}
	
	//let loaded movies play for 50ms to prime a few frames, then unmute
	if (movie_is_loaded && !movie_is_primed){
		if (millis() - movie_start_time > 50){
			gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PAUSED);
			movie_is_primed = true;
			mute_pipeline(false, &pipeline_active);
		}
	}
	
	//detect the end of a movie 
	if (movie_is_playing){
	
		static gint64 last_pos = ULONG_MAX;
		static int frame_count = 0;
		gint64 new_pos = get_position(&pipeline_active);
		if (new_pos == last_pos) frame_count++;
		else frame_count = 0;
		last_pos = new_pos;
		
		//if the video has 3 frames the same, set the video_done_flag and the app will request the portal to close
		if (frame_count > 2){
			printf("Video is EoF\n");
			gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PAUSED);
			movie_is_playing = false;
			if (video_done_flag != NULL) *video_done_flag = true;
		}
		
		//if the portal closes, stop the video
		if ((portal_state & PORTAL_OPEN_BIT) != PORTAL_OPEN_BIT) { 
				gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PAUSED);
				movie_is_playing = false;
				printf("Video done\n");
		}
	}
	
	//always immediately switch to new pipelines to be ready
	if (video_mode_requested != video_mode_current)
		start_pipeline();
}


void projector_logic_init(volatile bool *video_done_flag_p){

	video_done_flag = video_done_flag_p;

	//test patterns
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
	gstcontext_load_pipeline(GST_AGINGTV,		&pipeline[GST_AGINGTV]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert ! videoflip video-direction=3 	! queue ! agingtv       ! glupload ! glcolorconvert ! glvideoflip video-direction=1 ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_DICETV,		&pipeline[GST_DICETV]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert ! queue ! dicetv        ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_WARPTV,		&pipeline[GST_WARPTV]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert ! queue ! warptv        ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_SHAGADELICTV,	&pipeline[GST_SHAGADELICTV]	,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert ! queue ! shagadelictv  ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_VERTIGOTV,		&pipeline[GST_VERTIGOTV]	,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert ! queue ! vertigotv     ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_RIPPLETV,		&pipeline[GST_RIPPLETV]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert ! queue ! rippletv      ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_EDGETV,		&pipeline[GST_EDGETV]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert ! queue ! edgetv        ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_STREAKTV,		&pipeline[GST_STREAKTV]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert ! queue ! streaktv      ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_AATV,			&pipeline[GST_AATV]			,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert ! videoflip video-direction=3           ! queue ! aatv rain-mode=0 height=60 width=60 ! glupload ! glcolorconvert ! glvideoflip video-direction=1 ! glcolorscale ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_CACATV,		&pipeline[GST_CACATV]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! videoscale ! videoconvert ! video/x-raw,width=160,height=120,framerate=30/1,format=RGB ! videoflip video-direction=3 ! queue ! cacatv canvas-height=60 canvas-width=60  ! glupload ! glcolorconvert ! glvideoflip video-direction=1 ! glcolorscale ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");

	//gl effects 
	gstcontext_load_pipeline(GST_GLCUBE,		&pipeline[GST_GLCUBE]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! glfiltercube      ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_GLMIRROR,		&pipeline[GST_GLMIRROR]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! glvideoflip video-direction=3 ! gleffects_mirror  ! glvideoflip video-direction=1 ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_GLSQUEEZE,		&pipeline[GST_GLSQUEEZE]	,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_squeeze ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_GLSTRETCH,		&pipeline[GST_GLSTRETCH]	,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_stretch ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_GLTUNNEL,		&pipeline[GST_GLTUNNEL]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_tunnel  ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_GLTWIRL,		&pipeline[GST_GLTWIRL]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_twirl   ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_GLBULGE,		&pipeline[GST_GLBULGE]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_bulge   ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	gstcontext_load_pipeline(GST_GLHEAT,		&pipeline[GST_GLHEAT]		,GST_STATE_NULL,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_heat    ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");

	//spawn audio udp loopback to prevent re-init issues (bringing it back from portal gun v1.0)	
	system("gst-launch-1.0 alsasrc buffer-time=40000 ! audio/x-raw,layout=interleaved,rate=48000,format=S32LE,channels=2 ! udpsink host=127.0.0.1 port=5000 sync=false &");
	g_usleep(1000000);
	
	gstcontext_load_pipeline(GST_LIBVISUAL_CORONA,	&pipeline[GST_LIBVISUAL_CORONA]		,GST_STATE_NULL,"udpsrc buffer-size=1 port=5000 ! audio/x-raw,rate=48000,channels=2,format=S32LE ! queue max-size-time=10000000 leaky=downstream ! audioconvert !  libvisual_corona ! videoflip video-direction=1 		! video/x-raw,width=400,height=320,framerate=30/1 ! glupload ! glcolorconvert ! glcolorscale ! video/x-raw(memory:GLMemory),width=640,height=480 ! glfilterapp name=grabtexture ! fakesink sync=false async=false");
	gstcontext_load_pipeline(GST_LIBVISUAL_JESS,	&pipeline[GST_LIBVISUAL_JESS]		,GST_STATE_NULL,"udpsrc buffer-size=1 port=5000 ! audio/x-raw,rate=48000,channels=2,format=S32LE ! queue max-size-time=10000000 leaky=downstream ! audioconvert !  libvisual_jess      							  		! video/x-raw,width=400,height=320,framerate=30/1 ! glupload ! glcolorconvert ! glcolorscale ! video/x-raw(memory:GLMemory),width=640,height=480 ! glfilterapp name=grabtexture ! fakesink sync=false async=false");
	gstcontext_load_pipeline(GST_LIBVISUAL_INFINITE,&pipeline[GST_LIBVISUAL_INFINITE]	,GST_STATE_NULL,"udpsrc buffer-size=1 port=5000 ! audio/x-raw,rate=48000,channels=2,format=S32LE ! queue max-size-time=10000000 leaky=downstream ! audioconvert !  libvisual_infinite  							  		! video/x-raw,width=400,height=320,framerate=30/1 ! glupload ! glcolorconvert ! glcolorscale ! video/x-raw(memory:GLMemory),width=640,height=480 ! glfilterapp name=grabtexture ! fakesink sync=false async=false");
	gstcontext_load_pipeline(GST_LIBVISUAL_JAKDAW,	&pipeline[GST_LIBVISUAL_JAKDAW]		,GST_STATE_NULL,"udpsrc buffer-size=1 port=5000 ! audio/x-raw,rate=48000,channels=2,format=S32LE ! queue max-size-time=10000000 leaky=downstream ! audioconvert !  libvisual_jakdaw    							  		! video/x-raw,width=400,height=320,framerate=30/1 ! glupload ! glcolorconvert ! glcolorscale ! video/x-raw(memory:GLMemory),width=640,height=480 ! glfilterapp name=grabtexture ! fakesink sync=false async=false");
	gstcontext_load_pipeline(GST_LIBVISUAL_OINKSIE,	&pipeline[GST_LIBVISUAL_OINKSIE]	,GST_STATE_NULL,"udpsrc buffer-size=1 port=5000 ! audio/x-raw,rate=48000,channels=2,format=S32LE ! queue max-size-time=10000000 leaky=downstream ! audioconvert !  libvisual_oinksie   							  		! video/x-raw,width=400,height=320,framerate=30/1 ! glupload ! glcolorconvert ! glcolorscale ! video/x-raw(memory:GLMemory),width=640,height=480 ! glfilterapp name=grabtexture ! fakesink sync=false async=false");
	gstcontext_load_pipeline(GST_GOOM,&pipeline[GST_GOOM],GST_STATE_NULL,"udpsrc buffer-size=1 port=5000 ! audio/x-raw,rate=48000,channels=2,format=S32LE ! queue max-size-time=10000000 leaky=downstream ! audioconvert !  goom                							  		! video/x-raw,width=400,height=320,framerate=30/1 ! glupload ! glcolorconvert ! glcolorscale ! video/x-raw(memory:GLMemory),width=640,height=480 ! glfilterapp name=grabtexture ! fakesink sync=false async=false");
	gstcontext_load_pipeline(GST_GOOM2K1,&pipeline[GST_GOOM2K1] ,GST_STATE_NULL,"udpsrc buffer-size=1 port=5000 ! audio/x-raw,rate=48000,channels=2,format=S32LE ! queue max-size-time=10000000 leaky=downstream ! audioconvert !  goom2k1             							  		! video/x-raw,width=400,height=320,framerate=30/1 ! glupload ! glcolorconvert ! glcolorscale ! video/x-raw(memory:GLMemory),width=640,height=480 ! glfilterapp name=grabtexture ! fakesink sync=false async=false");		
	//SYNAE needs to load after WAVE, Wave does the class init, by playing and nulling this is assured.
	gstcontext_load_pipeline(GST_LIBVISUAL_WAVE,	&pipeline[GST_LIBVISUAL_WAVE]		,GST_STATE_PLAYING,"udpsrc buffer-size=1 port=5000 ! audio/x-raw,rate=48000,channels=2,format=S32LE ! queue max-size-time=10000000 leaky=downstream ! audioconvert !  wavescope style=3   							  		! video/x-raw,width=400,height=320,framerate=30/1 ! glupload ! glcolorconvert ! glcolorscale ! video/x-raw(memory:GLMemory),width=640,height=480 ! glfilterapp name=grabtexture ! fakesink sync=false async=false");
	g_usleep(1000000);
	gst_element_set_state (GST_ELEMENT (pipeline[GST_LIBVISUAL_WAVE]), GST_STATE_NULL);
	gstcontext_load_pipeline(GST_LIBVISUAL_SYNAE,	&pipeline[GST_LIBVISUAL_SYNAE]		,GST_STATE_PLAYING,"udpsrc buffer-size=1 port=5000 ! audio/x-raw,rate=48000,channels=2,format=S32LE ! queue max-size-time=10000000 leaky=downstream ! audioconvert ! synaescope shader=2	! videoflip video-direction=1 	! video/x-raw,width=400,height=320,framerate=30/1 ! glupload ! glcolorconvert ! glcolorscale ! video/x-raw(memory:GLMemory),width=640,height=480 ! glfilterapp name=grabtexture ! fakesink sync=false async=false");
	g_usleep(1000000);			
	gst_element_set_state (GST_ELEMENT (pipeline[GST_LIBVISUAL_SYNAE]), GST_STATE_NULL);
	
	gstcontext_load_pipeline(GST_MOVIE1,&pipeline[GST_MOVIE1],GST_STATE_PAUSED,"filesrc location=/home/pi/assets/movies/down/1.mp4 ! qtdemux name=dmux "
	"dmux.video_0 ! queue ! avdec_h264 ! queue ! videoconvert ! "
	"glupload ! glcolorscale ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA  ! glfilterapp name=grabtexture ! fakesink sync=true  "
	"dmux.audio_0 ! queue ! aacparse ! avdec_aac ! audioconvert ! volume name=vol  ! audio/x-raw,layout=interleaved,rate=48000,format=S32LE,channels=2 ! alsasink sync=true  device=dsp0");
	gstcontext_load_pipeline(GST_MOVIE2,&pipeline[GST_MOVIE2],GST_STATE_PAUSED,"filesrc location=/home/pi/assets/movies/down/2.mp4 ! qtdemux name=dmux "
	"dmux.video_0 ! queue ! avdec_h264 ! queue ! videoconvert ! "
	"glupload ! glcolorscale ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA  ! glfilterapp name=grabtexture ! fakesink sync=true  "
	"dmux.audio_0 ! queue ! aacparse ! avdec_aac ! audioconvert ! volume  name=vol ! audio/x-raw,layout=interleaved,rate=48000,format=S32LE,channels=2 ! alsasink sync=true  device=dsp0");
	gstcontext_load_pipeline(GST_MOVIE3,&pipeline[GST_MOVIE3],GST_STATE_PAUSED,"filesrc location=/home/pi/assets/movies/down/3.mp4 ! qtdemux name=dmux "
	"dmux.video_0 ! queue ! avdec_h264 ! queue ! videoconvert ! "
	"glupload ! glcolorscale ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA  ! glfilterapp name=grabtexture ! fakesink sync=true  "
	"dmux.audio_0 ! queue ! aacparse ! avdec_aac ! audioconvert ! volume  name=vol ! audio/x-raw,layout=interleaved,rate=48000,format=S32LE,channels=2 ! alsasink sync=true  device=dsp0");
	gstcontext_load_pipeline(GST_MOVIE4,&pipeline[GST_MOVIE4],GST_STATE_PAUSED,"filesrc location=/home/pi/assets/movies/down/4.mp4 ! qtdemux name=dmux "
	"dmux.video_0 ! queue ! avdec_h264 ! queue ! videoconvert ! "
	"glupload ! glcolorscale ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA  ! glfilterapp name=grabtexture ! fakesink sync=true  "
	"dmux.audio_0 ! queue ! aacparse ! avdec_aac ! audioconvert ! volume name=vol  ! audio/x-raw,layout=interleaved,rate=48000,format=S32LE,channels=2 ! alsasink sync=true  device=dsp0");
	gstcontext_load_pipeline(GST_MOVIE5,&pipeline[GST_MOVIE5],GST_STATE_PAUSED,"filesrc location=/home/pi/assets/movies/down/5.mp4 ! qtdemux name=dmux "
	"dmux.video_0 ! queue ! avdec_h264 ! queue ! videoconvert ! "
	"glupload ! glcolorscale ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA  ! glfilterapp name=grabtexture ! fakesink sync=true  "
	"dmux.audio_0 ! queue ! aacparse ! avdec_aac ! audioconvert ! volume name=vol  ! audio/x-raw,layout=interleaved,rate=48000,format=S32LE,channels=2 ! alsasink sync=true  device=dsp0");
	gstcontext_load_pipeline(GST_MOVIE6,&pipeline[GST_MOVIE6],GST_STATE_PAUSED,"filesrc location=/home/pi/assets/movies/down/6.mp4 ! qtdemux name=dmux "
	"dmux.video_0 ! queue ! avdec_h264 ! queue ! videoconvert ! "
	"glupload ! glcolorscale ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA  ! glfilterapp name=grabtexture ! fakesink sync=true  "
	"dmux.audio_0 ! queue ! aacparse ! avdec_aac ! audioconvert ! volume name=vol  ! audio/x-raw,layout=interleaved,rate=48000,format=S32LE,channels=2 ! alsasink sync=true  device=dsp0");
	gstcontext_load_pipeline(GST_MOVIE7,&pipeline[GST_MOVIE7],GST_STATE_PAUSED,"filesrc location=/home/pi/assets/movies/down/7.mp4 ! qtdemux name=dmux "
	"dmux.video_0 ! queue ! avdec_h264 ! queue ! videoconvert ! "
	"glupload ! glcolorscale ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA  ! glfilterapp name=grabtexture ! fakesink sync=true  "
	"dmux.audio_0 ! queue ! aacparse ! avdec_aac ! audioconvert ! volume  name=vol ! audio/x-raw,layout=interleaved,rate=48000,format=S32LE,channels=2 ! alsasink sync=true  device=dsp0");
	gstcontext_load_pipeline(GST_MOVIE8,&pipeline[GST_MOVIE8],GST_STATE_PAUSED,"filesrc location=/home/pi/assets/movies/down/8.mp4 ! qtdemux name=dmux "
	"dmux.video_0 ! queue ! avdec_h264 ! queue ! videoconvert ! "
	"glupload ! glcolorscale ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA  ! glfilterapp name=grabtexture ! fakesink sync=true  "
	"dmux.audio_0 ! queue ! aacparse ! avdec_aac ! audioconvert ! volume name=vol  ! audio/x-raw,layout=interleaved,rate=48000,format=S32LE,channels=2 ! alsasink sync=true  device=dsp0");
	gstcontext_load_pipeline(GST_MOVIE9,&pipeline[GST_MOVIE9],GST_STATE_PAUSED,"filesrc location=/home/pi/assets/movies/down/9.mp4 ! qtdemux name=dmux "
	"dmux.video_0 ! queue ! avdec_h264 ! queue ! videoconvert ! "
	"glupload ! glcolorscale ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA  ! glfilterapp name=grabtexture ! fakesink sync=true  "
	"dmux.audio_0 ! queue ! aacparse ! avdec_aac ! audioconvert ! volume name=vol  ! audio/x-raw,layout=interleaved,rate=48000,format=S32LE,channels=2 ! alsasink sync=true  device=dsp0");
	gstcontext_load_pipeline(GST_MOVIE10,&pipeline[GST_MOVIE10],GST_STATE_PAUSED,"filesrc location=/home/pi/assets/movies/down/10.mp4 ! qtdemux name=dmux "
	"dmux.video_0 ! queue ! avdec_h264 ! queue ! videoconvert ! "
	"glupload ! glcolorscale ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA  ! glfilterapp name=grabtexture ! fakesink sync=true  "
	"dmux.audio_0 ! queue ! aacparse ! avdec_aac ! audioconvert ! volume name=vol ! audio/x-raw,layout=interleaved,rate=48000,format=S32LE,channels=2 ! alsasink sync=true device=dsp0");
}	