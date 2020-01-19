#include "../shared.h"
#include <gst/gst.h>
#include <stdio.h>
#include <GL/glx.h>
#include <gst/gl/gl.h>
#include <gst/gl/x11/gstgldisplay_x11.h>
#include <unistd.h>  //getpid for priority change
#include <sys/resource.h> //setpriority
#include "portalgst.h"
#include "portalgl.h"
#include "ui.h"
#include "scene.h"

struct gun_struct *this_gun;

extern Display *dpy;
extern Window win;
extern GLXContext ctx;

static GstPad *outputpads[6];	
static GstElement *outputselector;
static GstPipeline *pipeline[GST_LAST+1],*pipeline_active;
static GstContext *x11context;
static GstContext *ctxcontext;

static volatile GLuint gst_shared_texture;

static GMainLoop *loop;

static int video_mode_requested = 0;
static int video_mode_current = -1;
static int portal_mode_requested = 9;
static int  ui_mode_requested = UI_ADVANCED;
static bool movieisplaying = false;

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
	if (GST_IS_ELEMENT(pipeline_active) || video_mode_current == GST_RPICAMSRC) {
		
		//leaving a audio effect mode 
		if (video_mode_current >= GST_LIBVISUAL_FIRST && video_mode_current <= GST_LIBVISUAL_LAST){
			//only pause  if we are going to a non audio mode from a audio mode, otherwise switch it live
			if (video_mode_requested < GST_LIBVISUAL_FIRST || video_mode_requested > GST_LIBVISUAL_LAST){
				gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PAUSED);
				printf("STOPPED alsa!\n\n");		
			} else {
				printf("alsa HOT SWAP!\n\n");
			}
		}		
		//leaving a movie mode 
		else if (video_mode_current >= GST_MOVIE_FIRST && video_mode_current <= GST_MOVIE_LAST){
			if (video_mode_requested < GST_MOVIE_FIRST || video_mode_requested > GST_MOVIE_LAST){
				gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PAUSED);
				printf("STOPPED video!\n\n");		
			}else{
				printf("video HOT SWAP!\n\n");
			}	
			movieisplaying = false;
		} else if (video_mode_current == GST_RPICAMSRC) {
			gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PAUSED);
			printf("PAUSE RPICAM!\n\n");
		} else {//null everything else	
			printf("NULL THE REST!\n\n");
			gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_NULL);
		}
	}
	
	//prep the new pipeline
	if (video_mode_requested >= GST_MOVIE_FIRST && video_mode_requested <= GST_MOVIE_LAST){
		pipeline_active = pipeline[GST_MOVIE_FIRST]; //all share the same pipeline
		//find file pos and go there
		int index = video_mode_requested - GST_MOVIE_FIRST;
		seek_to_time(movie_start_times[index]);  
		movieisplaying = true;
		
	}
	else if (video_mode_requested >= GST_LIBVISUAL_FIRST && video_mode_requested <= GST_LIBVISUAL_LAST){
		g_object_set (outputselector, "active-pad",  outputpads[video_mode_requested - GST_LIBVISUAL_FIRST], NULL);
		pipeline_active = pipeline[GST_LIBVISUAL_FIRST]; 
	} else {
		pipeline_active = pipeline[video_mode_requested]; 
	}
	
	//if we dont se the callbacks here, the bus request handler can do it
	//but explicitly setting it seems to be required when swapping pipelines
	gst_element_set_context (GST_ELEMENT (pipeline_active), ctxcontext);  			
	gst_element_set_context (GST_ELEMENT (pipeline_active), x11context);		

	//start the show
	gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PLAYING);	

	video_mode_current = video_mode_requested;
	
	printf("Pipeline %d changed to in %d milliseconds!\n",video_mode_current,millis() - start_time);
}

static gint64 get_position(void)
{
	gint64 pos;
	bool result = gst_element_query_position(GST_ELEMENT(pipeline[GST_MOVIE_FIRST]), GST_FORMAT_TIME, &pos);
	if (!result) pos = -1;
	return pos;
}

static gboolean idle_loop(gpointer data)
{
	static int accleration[3];
	
	portal_mode_requested = this_gun->portal_state;
	video_mode_requested  = this_gun->gst_state;
	ui_mode_requested = this_gun->ui_mode;

	if (movieisplaying){
		if (video_mode_current >= GST_MOVIE_FIRST && video_mode_current <= GST_MOVIE_LAST ){
			int index = video_mode_current - GST_MOVIE_FIRST;
			if (get_position() > movie_end_times[index]){
				printf("End of Chapter!\n");
				this_gun->video_done = true;
				gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PAUSED);
				movieisplaying = false;
			}
			if (portal_mode_requested != PORTAL_OPEN_ORANGE && portal_mode_requested != PORTAL_OPEN_BLUE ) {
				printf("End EARLY!\n");
				this_gun->video_done = true;
				gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PAUSED);
				movieisplaying = false;
			}
		}		
	}
	
	scene_animate(accleration,portal_mode_requested);
	
	if (ui_mode_requested == UI_HIDDEN_ADVANCED || ui_mode_requested == UI_HIDDEN_SIMPLE) {
	   scene_redraw(gst_shared_texture,portal_mode_requested,true);
	} else {
	   scene_redraw(gst_shared_texture,portal_mode_requested,false);
	   if (ui_mode_requested == UI_SIMPLE)   ui_redraw(true);
	   else  					   			 ui_redraw(false);
	}
	
	glXSwapBuffers(dpy, win);
	
	/* FPS counter */
	static uint32_t time_fps = 0;
	static int fps = 0;
	fps++;
	if (time_fps < millis()) {		
		printf("OPENGL FPS: %d \n",fps);
		fps = 0;
		time_fps += 1000;
		if (time_fps < millis()) time_fps = millis() + 1000;
	}
	
	if (video_mode_requested != video_mode_current)	{
		if (video_mode_requested >= GST_MOVIE_FIRST &&  video_mode_requested <= GST_MOVIE_LAST){
			if (portal_mode_requested == PORTAL_OPEN_ORANGE || portal_mode_requested == PORTAL_OPEN_BLUE ) { //wait until portal is opening to start video
				start_pipeline();
			}
		} else { //immediately change for all other pipelines to allow as much preloading as possible
			start_pipeline();
		}
	}
	//return true to automatically have this function called again when gstreamer is idle.
	return true;
}

//grabs the texture via the glfilterapp element
static gboolean drawCallback(GstElement* object, guint id , guint width ,guint height, gpointer user_data){
	
	gst_shared_texture = id;

	/* FPS counter */
	static uint32_t time_fps = 0;
	static int fps = 0;
	fps++;
	if (time_fps < millis()) {		
		printf("GSTREAMER FPS: %d \n",fps);
		fps = 0;
		time_fps += 1000;
		if (time_fps < millis()) time_fps = millis() + 1000;
	}

	return true;  //not sure why?
}

static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data)
{
	GMainLoop *loop = (GMainLoop*)data;

	switch (GST_MESSAGE_TYPE (msg))	{
	case GST_MESSAGE_EOS:
		g_print ("End-of-stream\n");
		this_gun->video_done = true;
		break;	
	case GST_MESSAGE_ERROR: //normal debug callback
		{
			gchar *debug = NULL;
			GError *err = NULL;
			gst_message_parse_error (msg, &err, &debug);
			g_print ("Error: %s\n", err->message);
			g_error_free (err);
			if (debug) {
				g_print ("Debug deails: %s\n", debug);
				g_free (debug);
			}
			g_main_loop_quit (loop);
			break;
		}
	case GST_MESSAGE_NEED_CONTEXT:  //THIS IS THE IMPORTANT PART
		{
			const gchar *context_type;
			gst_message_parse_context_type (msg, &context_type);
			if (g_strcmp0 (context_type, "gst.gl.app_context") == 0) {
				g_print("OpenGL Context Request Intercepted! %s\n", context_type);	
				gst_element_set_context (GST_ELEMENT (msg->src), ctxcontext);  			
			}
			if (g_strcmp0 (context_type, GST_GL_DISPLAY_CONTEXT_TYPE) == 0) {
				g_print("X11 Display Request Intercepted! %s\n", context_type);			
				gst_element_set_context (GST_ELEMENT (msg->src), x11context);			
			}
			break;
		}
	case GST_MESSAGE_HAVE_CONTEXT:
		g_print("This should never happen! Don't let the elements request their own context!\n");
	default:
		break;
	}

	return TRUE;
}

static void load_pipeline(int i, char * text)
{
	printf("Loading pipeline %d\n",i);
	
	pipeline[i] = GST_PIPELINE(gst_parse_launch(text, NULL));

	//set the bus watcher for error handling and to pass the x11 display and opengl context when the elements request it
	//must be BEFORE setting the client-draw callback
	GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE (pipeline[i]));
	gst_bus_add_watch(bus, bus_call, loop);
	gst_object_unref(bus);
	
	//set the glfilterapp callback that will capture the textures
	//do this AFTER attaching the bus handler so context can be set
	GstElement *grabtexture = gst_bin_get_by_name(GST_BIN (pipeline[i]), "grabtexture");
	g_signal_connect(grabtexture, "client-draw",  G_CALLBACK (drawCallback), NULL);
	gst_object_unref(grabtexture);	
	
	gst_element_set_context(GST_ELEMENT (pipeline[i]), ctxcontext);  			
	gst_element_set_context(GST_ELEMENT (pipeline[i]), x11context);

	if (i == GST_MOVIE_FIRST || i == GST_LIBVISUAL_FIRST || i == GST_RPICAMSRC) {
		gst_element_set_state(GST_ELEMENT (pipeline[i]), GST_STATE_PAUSED);
	}
}

void portalgst_init(void)
{
	//set priority for the opengl engine and video output
	setpriority(PRIO_PROCESS, getpid(), -10);
	
	shared_init(&this_gun,false);
	ui_init();	
	scene_init();
	
	/* Initialize GStreamer */
	static char *arg1_gst[]  = {"gstvideo"}; 
	static char *arg2_gst[]  = {"--gst-disable-registry-update"};  //dont rescan the registry to load faster.
	static char *arg3_gst[]  = {"--gst-debug-level=1"};  //dont show debug messages
	static char **argv_gst[] = {arg1_gst,arg2_gst,arg3_gst};
	int argc_gst = sizeof(argv_gst)/sizeof(char *);
	gst_init (&argc_gst, argv_gst);
	
	//get x11context ready for handing off to elements in the callback
	GstGLDisplay * gl_display = GST_GL_DISPLAY (gst_gl_display_x11_new_with_display (dpy));//dpy is the glx OpenGL display			
	x11context = gst_context_new (GST_GL_DISPLAY_CONTEXT_TYPE, TRUE);
	gst_context_set_gl_display(x11context, gl_display);
	
	//get ctxcontext ready for handing off to elements in the callback
	GstGLContext *gl_context = gst_gl_context_new_wrapped ( gl_display, (guintptr) ctx,GST_GL_PLATFORM_GLX,GST_GL_API_OPENGL); //ctx is the glx OpenGL context
	ctxcontext = gst_context_new ("gst.gl.app_context", TRUE);
	gst_structure_set (gst_context_writable_structure (ctxcontext), "context", GST_TYPE_GL_CONTEXT, gl_context, NULL);
	
	//preload all pipelines we will be switching between.  This allows faster switching than destroying and recrearting the pipelines
	//I've noticed that if too many pipelines get destroyed (gst_object_unref) and recreated  gstreamer & x11 will eventually crash with context errors
	//this can switch between pipelines in 5-20ms on a Pi3, which is quick enough for the human eye
	
	//test patterns
	load_pipeline(GST_BLANK ,"videotestsrc pattern=2 ! video/x-raw,width=640,height=480,framerate=(fraction)1/1 ! queue ! glupload ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true");
	load_pipeline(GST_VIDEOTESTSRC ,"videotestsrc ! video/x-raw,width=640,height=480,framerate=(fraction)30/1 ! queue ! glupload ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true");
	load_pipeline(GST_VIDEOTESTSRC_CUBED,"videotestsrc ! video/x-raw,width=640,height=480,framerate=(fraction)30/1 ! queue ! glupload ! glfiltercube ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true");
	
	//camera launch 192.168.1.20 gordon    192.168.1.21 chell
	if(this_gun->gordon){
		load_pipeline(GST_RPICAMSRC ,"rpicamsrc preview=0 ! image/jpeg,width=640,height=480,framerate=30/1 ! "
		"queue max-size-time=50000000 leaky=upstream ! jpegparse ! tee name=t "
		"t. ! queue ! rtpjpegpay ! udpsink host=192.168.3.21 port=9000 sync=false "
		"t. ! queue ! jpegdec ! videorate ! video/x-raw,framerate=10/1 ! videoscale ! video/x-raw,width=400,height=240 ! videoflip method=3 ! jpegenc ! multifilesink location=/var/www/html/tmp/snapshot.jpg sync=false "
		"t. ! queue ! jpegdec ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false"
		);  //the order of this matters, the fakesink MUST Be last in the tee chain!
	}else {
		load_pipeline(GST_RPICAMSRC ,"rpicamsrc preview=0 ! image/jpeg,width=640,height=480,framerate=30/1 ! "
		"queue max-size-time=50000000 leaky=upstream ! jpegparse ! tee name=t "
		"t. ! queue ! rtpjpegpay ! udpsink host=192.168.3.20 port=9000 sync=false "
		"t. ! queue ! jpegdec ! videorate ! video/x-raw,framerate=10/1 ! videoscale ! video/x-raw,width=400,height=240 ! videoflip method=3 ! jpegenc ! multifilesink location=/var/www/html/tmp/snapshot.jpg sync=false "
		"t. ! queue ! jpegdec ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false"
		);  //the order of this matters, the fakesink MUST Be last in the tee chain!
	}	

	//normal
	load_pipeline(GST_NORMAL ,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	
	//cpu effects drop framerate to 20 from 30 due to cpu usage
	load_pipeline(GST_RADIOACTV    ,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! video/x-raw,framerate=20/1 ! videoconvert ! queue ! radioactv     ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_REVTV        ,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! video/x-raw,framerate=20/1 ! videoconvert ! queue ! revtv         ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_AGINGTV      ,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! video/x-raw,framerate=20/1 ! videoconvert ! queue ! agingtv       ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_DICETV       ,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! video/x-raw,framerate=20/1 ! videoconvert ! queue ! dicetv        ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_WARPTV       ,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! video/x-raw,framerate=20/1 ! videoconvert ! queue ! warptv        ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_SHAGADELICTV ,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! video/x-raw,framerate=20/1 ! videoconvert ! queue ! shagadelictv  ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_VERTIGOTV    ,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! video/x-raw,framerate=20/1 ! videoconvert ! queue ! vertigotv     ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_RIPPLETV     ,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! video/x-raw,framerate=20/1 ! videoconvert ! queue ! rippletv      ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_EDGETV       ,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! video/x-raw,framerate=20/1 ! videoconvert ! queue ! edgetv        ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_STREAKTV     ,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! video/x-raw,framerate=20/1 ! videoconvert ! queue ! streaktv      ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_AATV         ,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert                                                                                       ! queue ! aatv rain-mode=0 ! glupload ! glcolorconvert ! glcolorscale ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_CACATV       ,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! videoscale ! videoconvert ! video/x-raw,width=160,height=120,framerate=20/1,format=RGB ! queue ! cacatv           ! glupload ! glcolorconvert ! glcolorscale ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");

	//gl effects 
	load_pipeline(GST_GLCUBE   ,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! glfiltercube      ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_GLMIRROR ,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_mirror  ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_GLSQUEEZE,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_squeeze ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_GLSTRETCH,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_stretch ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_GLTUNNEL ,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_tunnel  ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_GLTWIRL  ,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_twirl   ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_GLBULGE  ,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_bulge   ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_GLHEAT   ,"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_heat    ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	
	//audio effects - alsasrc takes a second or so to init, so here a output-selector is used
	//effect order matters since pads on the output selector can't easily be named in advance 
	//audio format must match the movie output stuff, otherwise the I2S Soundcard will get slow and laggy when switching formats!
	load_pipeline(GST_LIBVISUAL_FIRST,"alsasrc buffer-time=40000 ! audio/x-raw,layout=interleaved,rate=48000,format=S32LE,channels=2 ! queue max-size-time=10000000 leaky=downstream ! audioconvert ! "
	"output-selector name=audioin pad-negotiation-mode=Active "
	"audioin. ! libvisual_jess     ! videosink. "
	"audioin. ! libvisual_infinite ! videosink. "
	"audioin. ! libvisual_jakdaw   ! videosink. "
	"audioin. ! libvisual_oinksie  ! videosink. "
	"audioin. ! goom               ! videosink. "
	"audioin. ! goom2k1            ! videosink. "
	"funnel name=videosink ! video/x-raw,width=400,height=320,framerate=30/1 ! "
	"glupload ! glcolorconvert ! glcolorscale ! video/x-raw(memory:GLMemory),width=640,height=480 ! "
	"glfilterapp name=grabtexture ! fakesink sync=false async=false");
	
	//movie pipeline, has all videos as long long video, chapter start and end times stored in gstvideo.h
	//it doesnt work well to load and unload various input files due to the 
	//audio format must match the visual input stuff, otherwise the I2S Soundcard will get slow and laggy when switching formats! 
	load_pipeline(GST_MOVIE_FIRST ,"filesrc location=/home/pi/assets/movies/all.mp4 ! qtdemux name=dmux "
	"dmux.video_0 ! queue ! avdec_h264 ! queue ! videoconvert ! "
	"glupload ! glcolorscale ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true async=false "
	"dmux.audio_0 ! queue ! aacparse ! avdec_aac ! audioconvert ! audio/x-raw,layout=interleaved,rate=48000,format=S32LE,channels=2 ! alsasink sync=true async=false device=dmix");
	//"dmux.audio_0 ! fakesink");  //disable sound for testing on the workbench

	//save the output pads from the visualization pipelines
	//get the output-selector element
	outputselector = gst_bin_get_by_name(GST_BIN (pipeline[GST_LIBVISUAL_FIRST]), "audioin");
	
	//save each output pad for later
	outputpads[0] = gst_element_get_static_pad(outputselector,"src_0");
	outputpads[1] = gst_element_get_static_pad(outputselector,"src_1");
	outputpads[2] = gst_element_get_static_pad(outputselector,"src_2");
	outputpads[3] = gst_element_get_static_pad(outputselector,"src_3");
	outputpads[4] = gst_element_get_static_pad(outputselector,"src_4");
	outputpads[5] = gst_element_get_static_pad(outputselector,"src_5");
	
	//start the idle and main loops
	loop = g_main_loop_new (NULL, FALSE);
	//gpointer data = NULL;
	//g_idle_add (idle_loop, data);
	//g_timeout_add (10,idle_loop ,pipeline); 
	g_timeout_add_full ( G_PRIORITY_HIGH,6,idle_loop ,pipeline,NULL); 
	g_main_loop_run (loop); //let gstreamer's GLib event loop take over
}