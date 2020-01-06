//#define GST_USE_UNSTABLE_API  //if you're not out of control you're not in control

#include "gstvideo.h"
#include <sys/resource.h>
#include <gst/gst.h>
#include <gst/gl/gl.h>
#include <gst/gl/x11/gstgldisplay_x11.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>

#include <GL/gl.h>
#include <GL/glx.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <iostream>
#include <string>

#include "model_board/model_board.h"
#include "BitmapFontClass.h"

int parentpid = 0;
int raspivid_PID = 0;

int input_command_pipe;
Display *dpy;
Window win;
GLXContext ctx;
CBitmapFont FontNormal;

GstPad *outputpads[6];	
GstElement *outputselector;
GstPipeline *pipeline[75],*pipeline_active;
GstContext *x11context;
GstContext *ctxcontext;

GLuint normal_texture;
volatile GLuint gst_shared_texture;

GMainLoop *loop;

#ifndef GLX_MESA_swap_control
#define GLX_MESA_swap_control 1
typedef int (*PFNGLXGETSWAPINTERVALMESAPROC)(void);
#endif

/* return current time (in seconds) */
static double current_time(void) {
	struct timeval tv;
	struct timezone tz;
	(void) gettimeofday(&tv, &tz);
	return (double) tv.tv_sec + tv.tv_usec / 1000000.0;
}

unsigned int millis (void){
  struct  timespec ts ;

  clock_gettime (CLOCK_MONOTONIC_RAW, &ts) ;
  uint64_t now  = (uint64_t)ts.tv_sec * (uint64_t)1000 + (uint64_t)(ts.tv_nsec / 1000000L) ;

  return (uint32_t)now ;
}

/* new window size or exposure */
static void reshape(int screen_width, int screen_height)
{
	float nearp = 1, farp = 100.0f, hht, hwd;

	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glViewport(0, 0, (GLsizei)screen_width, (GLsizei)screen_height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	hht = nearp * tan(45.0 / 2.0 / 180.0 * M_PI);
	hwd = hht * screen_width / screen_height;

	glFrustum(-hwd, hwd, -hht, hht, nearp, farp);
	
}

void video_ended(){
	if (parentpid != 0)	kill(parentpid,SIGUSR2);
}

static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer data){
	GMainLoop *loop = (GMainLoop*)data;

	switch (GST_MESSAGE_TYPE (msg))
	{
	case GST_MESSAGE_EOS:
		g_print ("End-of-stream\n");
		video_ended();
		break;	
	case GST_MESSAGE_ERROR: //normal debug callback
		{
			gchar *debug = NULL;
			GError *err = NULL;

			gst_message_parse_error (msg, &err, &debug);

			g_print ("Error: %s\n", err->message);
			g_error_free (err);

			if (debug)
			{
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
			if (g_strcmp0 (context_type, "gst.gl.app_context") == 0) 
			{
				g_print("OpenGL Context Request Intercepted! %s\n", context_type);	
				gst_element_set_context (GST_ELEMENT (msg->src), ctxcontext);  			
			}
			if (g_strcmp0 (context_type, GST_GL_DISPLAY_CONTEXT_TYPE) == 0) 
			{
				g_print("X11 Display Request Intercepted! %s\n", context_type);			
				gst_element_set_context (GST_ELEMENT (msg->src), x11context);			
			}

			break;
		}
	case GST_MESSAGE_HAVE_CONTEXT:
		{
			g_print("This should never happen! Don't let the elements set their own context!\n");
			
		}
	default:
		break;
	}

	return TRUE;
}


/*
* Create an RGB, double-buffered window.
* Return the window and context handles.
*/
static void make_window( Display *dpy, const char *name,int x, int y, int width, int height,Window *winRet, GLXContext *ctxRet, VisualID *visRet) {
	int attribs[64];
	int i = 0;

	int scrnum;
	XSetWindowAttributes attr;
	unsigned long mask;
	Window root;
	Window win;

	XVisualInfo *visinfo;

	/* Singleton attributes. */
	attribs[i++] = GLX_RGBA;
	attribs[i++] = GLX_DOUBLEBUFFER;

	/* Key/value attributes. */
	attribs[i++] = GLX_RED_SIZE;
	attribs[i++] = 1;
	attribs[i++] = GLX_GREEN_SIZE;
	attribs[i++] = 1;
	attribs[i++] = GLX_BLUE_SIZE;
	attribs[i++] = 1;
	attribs[i++] = GLX_DEPTH_SIZE;
	attribs[i++] = 1;

	attribs[i++] = None;

	scrnum = DefaultScreen( dpy );
	root = RootWindow( dpy, scrnum );

	visinfo = glXChooseVisual(dpy, scrnum, attribs);
	if (!visinfo) {
		printf("Error: couldn't get an RGB, Double-buffered");
		exit(1);
	}

	/* window attributes */
	attr.background_pixel = 0;
	attr.border_pixel = 0;
	attr.colormap = XCreateColormap( dpy, root, visinfo->visual, AllocNone);
	attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
	/* XXX this is a bad way to get a borderless window! */
	mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

	win = XCreateWindow( dpy, root, x, y, width, height,0, visinfo->depth, InputOutput,	visinfo->visual, mask, &attr );

	/* set hints and properties */
	{
		XSizeHints sizehints;
		sizehints.x = x;
		sizehints.y = y;
		sizehints.width  = width;
		sizehints.height = height;
		sizehints.flags = USSize | USPosition;
		XSetNormalHints(dpy, win, &sizehints);
		XSetStandardProperties(dpy, win, name, name,
		None, (char **)NULL, 0, &sizehints);
	}

	ctx = glXCreateContext( dpy, visinfo, NULL, True );
	if (!ctx) {
		printf("Error: glXCreateContext failed\n");
		exit(1);
	}

	*winRet = win;
	*ctxRet = ctx;
	*visRet = visinfo->visualid;

	XFree(visinfo);
}

/**
* Determine whether or not a GLX extension is supported.
*/
static int is_glx_extension_supported(Display *dpy, const char *query)
{
	const int scrnum = DefaultScreen(dpy);
	const char *glx_extensions = NULL;
	const size_t len = strlen(query);
	const char *ptr;

	if (glx_extensions == NULL) {
		glx_extensions = glXQueryExtensionsString(dpy, scrnum);
	}

	ptr = strstr(glx_extensions, query);
	return ((ptr != NULL) && ((ptr[len] == ' ') || (ptr[len] == '\0')));
}

/**
* Attempt to determine whether or not the display is synched to vblank.
*/
static void query_vsync(Display *dpy, GLXDrawable drawable)
{
	int interval = 0;

#if defined(GLX_EXT_swap_control)
	if (is_glx_extension_supported(dpy, "GLX_EXT_swap_control")) {
		unsigned int tmp = -1;
		glXQueryDrawable(dpy, drawable, GLX_SWAP_INTERVAL_EXT, &tmp);
		interval = tmp;
	} else
#endif
	if (is_glx_extension_supported(dpy, "GLX_MESA_swap_control")) {
		PFNGLXGETSWAPINTERVALMESAPROC pglXGetSwapIntervalMESA =
		(PFNGLXGETSWAPINTERVALMESAPROC)
		glXGetProcAddressARB((const GLubyte *) "glXGetSwapIntervalMESA");

		interval = (*pglXGetSwapIntervalMESA)();
	} else if (is_glx_extension_supported(dpy, "GLX_SGI_swap_control")) {
		/* The default swap interval with this extension is 1.  Assume that it
	* is set to the default.
	*
	* Many Mesa-based drivers default to 0, but all of these drivers also
	* export GLX_MESA_swap_control.  In that case, this branch will never
	* be taken, and the correct result should be reported.
	*/
		interval = 1;
	}


	if (interval > 0) {
		printf("Running synchronized to the vertical refresh.  The framerate should be\n");
		if (interval == 1) {
			printf("approximately the same as the monitor refresh rate.\n");
		} else if (interval > 1) {
			printf("approximately 1/%d the monitor refresh rate.\n",
			interval);
		}
	}
}

static void seek_to_time ( gint64 time_nanoseconds){
	if (!gst_element_seek ( GST_ELEMENT(pipeline[GST_MOVIE_FIRST]), 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, time_nanoseconds, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
		g_print ("Seek failed!\n");
	}
}
/*
waiting for X server to shut down (II) Server terminated successfully (0). Closing log file.
*/
gint64 get_position(){
	gint64 pos;
	bool result = gst_element_query_position ( GST_ELEMENT(pipeline[GST_MOVIE_FIRST]), GST_FORMAT_TIME, &pos);
	if (!result) pos = -1;
	return pos;
}

//grabs the texture via the glfilterapp element
gboolean drawCallback (GstElement* object, guint id , guint width ,guint height, gpointer user_data){

	static GTimeVal current_time;
	static glong last_sec = current_time.tv_sec;
	static gint nbFrames = 0;
	
	//printf("Texture #:%d  X:%d  Y:%d!\n",id,width,height);
	//snapshot(normal_texture);
	
	gst_shared_texture = id;
	g_get_current_time (&current_time);
	nbFrames++;

	if ((current_time.tv_sec - last_sec) >= 1)
	{
		get_position();
		std::cout << "GSTREAMER FPS = " << nbFrames << std::endl;
		nbFrames = 0;
		last_sec = current_time.tv_sec;
	}
	
	return true;  //not sure why?
}

void load_pipeline(int i, char * text){
	
	printf("Loading pipeline %d\n",i);
	
	pipeline[i] = GST_PIPELINE (gst_parse_launch(text, NULL));

	//set the bus watcher for error handling and to pass the x11 display and opengl context when the elements request it
	//must be BEFORE setting the client-draw callback
	GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline[i]));
	gst_bus_add_watch (bus, bus_call, loop);
	gst_object_unref (bus);
	
	//set the glfilterapp callback that will capture the textures
	//do this AFTER attaching the bus handler so context can be set
	//if (i != GST_RPICAMSRC){  //the camera bus has no texture output  //NOT ANY MORE!
		GstElement *grabtexture = gst_bin_get_by_name (GST_BIN (pipeline[i]), "grabtexture");
		g_signal_connect (grabtexture, "client-draw",  G_CALLBACK (drawCallback), NULL);
		gst_object_unref (grabtexture);	
	//}
	
	//warm up all of the pipelines that stay around
	if ( i == GST_RPICAMSRC ){
		gst_element_set_state (GST_ELEMENT (pipeline[i]), GST_STATE_READY);
	}else if (i == GST_MOVIE_FIRST || i == GST_LIBVISUAL_FIRST ){
		//special warmup to preload the video stream so seeking always
		//context must be assigned before going paused 
		gst_element_set_context (GST_ELEMENT (pipeline[i]), ctxcontext);  			
		gst_element_set_context (GST_ELEMENT (pipeline[i]), x11context);
		gst_element_set_state (GST_ELEMENT (pipeline[i]), GST_STATE_PAUSED);
	}
}


static int video_mode_requested = 0;
static int video_mode_current = -1;
static int portal_mode_requested = 9;

static bool movieisplaying = false;

void start_pipeline(){
	
	double start_time = current_time();
	
	//stop the old pipeline
	if (GST_IS_ELEMENT(pipeline_active) || video_mode_current == GST_RPICAMSRC) {
		
		//if we are leaving a audio effect mode 
		if (video_mode_current >= GST_LIBVISUAL_FIRST && video_mode_current <= GST_LIBVISUAL_LAST){
			//only pause  if we are going to a non audio mode from a audio mode, otherwise switch it live
			if (video_mode_requested < GST_LIBVISUAL_FIRST || video_mode_requested > GST_LIBVISUAL_LAST){
				gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PAUSED);
				printf("STOPPED alsa!\n\n");		
			}else{
				printf("alsa HOT SWAP!\n\n");
			}
		}		
		//if its a video stream, unload it completely
		else if (video_mode_current >= GST_MOVIE_FIRST && video_mode_current <= GST_MOVIE_LAST){
			if (video_mode_requested < GST_MOVIE_FIRST || video_mode_requested > GST_MOVIE_LAST){
				gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PAUSED);
				printf("STOPPED video!\n\n");		
			}else{
				printf("video HOT SWAP!\n\n");
			}	
			movieisplaying = false;
		}
		else if (video_mode_current == GST_RPICAMSRC ) {
		gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PAUSED);
			printf("PAUSE RPICAM!\n\n");
		}else{//null everything else	
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
	}else{
		pipeline_active = pipeline[video_mode_requested]; 
	}
	
	//if we dont se the callbacks here, the bus request handler can do it
	//but explicitly setting it seems to be required when swapping pipelines
		gst_element_set_context (GST_ELEMENT (pipeline_active), ctxcontext);  			
		gst_element_set_context (GST_ELEMENT (pipeline_active), x11context);		

		//start the show
		gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PLAYING);	

	video_mode_current = video_mode_requested;
	
	printf("Pipeline %d changed to in %f seconds!\n",video_mode_current,current_time() - start_time);
}


void print_centered(char * input,int height){
	int offset = ((768/2) - FontNormal.GetWidth(input) )/2;
	FontNormal.Print(input,offset,height);
}

GLuint text_vertex_list;

void init_text_background(){

	text_vertex_list = glGenLists( 1 );
	glNewList( text_vertex_list, GL_COMPILE );
	glBegin( GL_QUADS );
	
	glVertex3f( 1366/2,  768/2,0);//top left
	glVertex3f( 1366/2,0,0);//bottom left
	glVertex3f(  1366,0,0);//bottom right	
	glVertex3f(  1366, 768/2,0);//top right
	glEnd( );
	glEndList();
	
}

void print_text_overlay(){
	
		
	  glDisable(GL_DEPTH_TEST);
  glDepthMask(false);
  
  // Setup projection
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  // Setup Modelview
 glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  
//set size here
  glOrtho(0,1366,0,768/2,-1,1);
  
glBindTexture(GL_TEXTURE_2D, 0); //no texture	
	glColor4f(0.0,0.5,0.5,1.0); 
	glCallList(text_vertex_list);
	
glTranslatef(1366 , 0, 0);
	  
  		
  //angle here
glRotatef(90.0, 0.0, 0.0, 1.0);



	
	
  // Setup Texture, color and blend options
  glEnable(GL_TEXTURE_2D);
  
	FontNormal.Bind();
	 FontNormal.SetBlend();
	 
 
  
  
  			uint_fast32_t current_time = millis();
			uint_fast32_t milliseconds = (current_time % 1000);
			uint_fast32_t seconds      = (current_time / 1000) % 60;
			uint_fast32_t minutes      =((current_time / (1000*60)) % 60);
			uint_fast32_t hours        =((current_time / (1000*60*60)) % 24);
			
    char temp[200];

    sprintf(temp,"Gordon");	
    print_centered(temp,550);
	
        sprintf(temp,"Synced");	
    print_centered(temp,100 + 80*4);
	
	      sprintf(temp,"12.4V");	
    print_centered(temp,100 + 80*3);
	
    sprintf(temp,"74/120\260F");	
    print_centered(temp,100 + 80*2);
	   sprintf(temp,"1.34ms");	
    print_centered(temp,100 + 80*1);
   sprintf(temp,"Solo Mode");	
    print_centered(temp,64);
	
   sprintf(temp,"%.2d:%.2d:%.2d.%.3d",hours,minutes ,seconds,milliseconds);	
    print_centered(temp,0);
	
 glMatrixMode(GL_PROJECTION);
 glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  
  glEnable(GL_DEPTH_TEST);
  glDepthMask(true);
  glEnable(GL_BLEND); 
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
}

static gboolean idle_loop (gpointer data) {
	
	static int accleration[3];
	
	//opengl rendering update
	static int frames = 0;
	static double tRate0 = -1.0;
	double t = current_time();

	//read as much as we can
	while (1){
		
		#define buf_len 100
		static int buf_index = 0;
		char buffer[buf_len];
		
		int characters_read = read(input_command_pipe, &buffer[buf_index], 1);
		//printf("\n-%d-\n",characters_read);
		if (characters_read <= 0){ //done!
			//printf("Done.");
			break; 
		}else{
			buf_index++;
		}
		
		if (buf_index >= buf_len - 1) { //overflow
			buf_index = 0;  
			printf("\nOverflow..\n");
		}
		
		if (buf_index > 0){
			if (buffer[buf_index-1] == '\n'){  //parse it!
				
				buffer[buf_index] = '\0'; //replace newline with null to terminate string  
				//printf("\nParsing..\n");
				//printf("\n%s\n",buffer);
				
				int temp[5];
				int result = sscanf(buffer,"%d %d %d %d %d", &temp[0],&temp[1],&temp[2],&temp[3],&temp[4]);
				if (result != 5){
					fprintf(stderr, "Unrecognized input with %d items.\n", result);
				}else{
					portal_mode_requested = temp[0];
					video_mode_requested  = temp[1];
					accleration[0]        = temp[2];
					accleration[1]        = temp[3];
					accleration[2]        = temp[4];
				}
				
				buf_index = 0;
			}
		}
	}
	//printf("\ncycle..\n");
	if (movieisplaying){
		if (video_mode_current >= GST_MOVIE_FIRST && video_mode_current <= GST_MOVIE_LAST ){
			int index = video_mode_current - GST_MOVIE_FIRST;
			if (get_position() > movie_end_times[index]){
				printf("End of Chapter!\n");
				video_ended();
				gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PAUSED);
				movieisplaying = false;
			}
			if (portal_mode_requested != AHRS_OPEN_ORANGE &&portal_mode_requested != AHRS_OPEN_BLUE ) {
				printf("End EARLY!\n");
				video_ended();
				gst_element_set_state (GST_ELEMENT (pipeline_active), GST_STATE_PAUSED);
				movieisplaying = false;
			}
			
		}
		
		
	}
	
	model_board_animate(accleration,portal_mode_requested);
	model_board_redraw(gst_shared_texture,portal_mode_requested);
	
	print_text_overlay();

  

	glXSwapBuffers(dpy, win);
	
	frames++;

	if (tRate0 < 0.0)
	tRate0 = t;
	if (t - tRate0 >= 5.0) {
		GLfloat seconds = t - tRate0;
		GLfloat fps = frames / seconds;
		printf("Rendering %d frames in %3.1f seconds = %6.3f FPS\n", frames, seconds,fps);
		fflush(stdout);
		tRate0 = t;
		frames = 0;
	}
	
	if (video_mode_requested != video_mode_current)	{
		if (video_mode_requested >= GST_MOVIE_FIRST &&  video_mode_requested <= GST_MOVIE_LAST){
			if (portal_mode_requested == AHRS_OPEN_ORANGE || portal_mode_requested == AHRS_OPEN_BLUE ) { //wait until portal is opening to start video
				start_pipeline();
			}
		}
		else{ //immediately change for all other pipelines to allow as much preloading as possible
			start_pipeline();
		}
	}
	//return true to automatically have this function called again when gstreamer is idle.
	return true;
}


int main(int argc, char *argv[]){
		
	//set priority for the opengl engine and video output
	
	setpriority(PRIO_PROCESS, getpid(), -10);
	
	if (argc < 2){
		printf("GSTVIDEO: No Parent Pid Supplied, EoS signaling disabled!\n");
	}else{
		parentpid = atoi(argv[1]);
	}
	printf("GSTVIDEO: Using PID %d!\n",parentpid);
	
	mkfifo ("/home/pi/GSTVIDEO_IN_PIPE", 0777 );
	system("chown pi /home/pi/GSTVIDEO_IN_PIPE");
	
	//OPEN PIPE WITH READ ONLY
	if ((input_command_pipe = open ("/home/pi/GSTVIDEO_IN_PIPE",  ( O_RDONLY | O_NONBLOCK ) ))<0){
		perror("GSTVIDEO: Could not open named pipe for reading.");
		exit(-1);
	}
	printf("GSTVIDEO: /home/pi/GSTVIDEO_IN_PIPE has been opened.\n");

	fcntl(input_command_pipe, F_SETFL, fcntl(input_command_pipe, F_GETFL, 0) | O_NONBLOCK);
	
	/* Initialize X11 */
	//unsigned int winWidth = 720, winHeight = 480;
	unsigned int winWidth = 1366 * 2, winHeight = 768;
	
	int x = 0, y = 0;

	char *dpyName = NULL;
	GLboolean printInfo = GL_FALSE;
	VisualID visId;
	
	dpy = XOpenDisplay(dpyName);
	if (!dpy) {
		printf("Error: couldn't open display %s\n",
		dpyName ? dpyName : getenv("DISPLAY"));
		return -1;
	}

	make_window(dpy, "gstvideo", x, y, winWidth, winHeight, &win, &ctx, &visId);
	XMapWindow(dpy, win);
	glXMakeCurrent(dpy, win, ctx);
	query_vsync(dpy, win);

	/* Inspect the texture */
	//snapshot(normal_texture);
	
	if (printInfo) {
		printf("GL_RENDERER   = %s\n", (char *) glGetString(GL_RENDERER));
		printf("GL_VERSION    = %s\n", (char *) glGetString(GL_VERSION));
		printf("GL_VENDOR     = %s\n", (char *) glGetString(GL_VENDOR));
		printf("GL_EXTENSIONS = %s\n", (char *) glGetString(GL_EXTENSIONS));
		printf("VisualID %d, 0x%x\n", (int) visId, (int) visId);
	}

	/* Set initial projection/viewing transformation.
	* We can't be sure we'll get a ConfigureNotify event when the window
	* first appears.
	*/
	reshape(winWidth, winHeight);
	
	/* Initialize GStreamer */
	const char *arg1_gst[]  = {"gstvideo"}; 
	const char *arg2_gst[]  = {"--gst-disable-registry-update"};  //dont rescan the registry to load faster.
	const char *arg3_gst[]  = {"--gst-debug-level=0"};  //dont show debug messages
	char ** argv_gst[3] = {(char **)arg1_gst,(char **)arg2_gst,(char **)arg3_gst};
	int argc_gst = 3;
	gst_init (&argc_gst, argv_gst );
	
	//get x11context ready for handing off to elements in the callback
	GstGLDisplay * gl_display = GST_GL_DISPLAY (gst_gl_display_x11_new_with_display (dpy));//dpy is the glx OpenGL display			
	x11context = gst_context_new (GST_GL_DISPLAY_CONTEXT_TYPE, TRUE);
	gst_context_set_gl_display (x11context, gl_display);
	
	//get ctxcontext ready for handing off to elements in the callback
	GstGLContext *gl_context = gst_gl_context_new_wrapped ( gl_display, (guintptr) ctx,GST_GL_PLATFORM_GLX,GST_GL_API_OPENGL); //ctx is the glx OpenGL context
	ctxcontext = gst_context_new ("gst.gl.app_context", TRUE);
	gst_structure_set (gst_context_writable_structure (ctxcontext), "context", GST_TYPE_GL_CONTEXT, gl_context, NULL);
	
	//preload all pipelines we will be switching between.  This allows faster switching than destroying and recrearting the pipelines
	//I've noticed that if too many pipelines get destroyed (gst_object_unref) and recreated  gstreamer & x11 will eventually crash with context errors
	//this can switch between pipelines in 5-20ms on a Pi3, which is quick enough for the human eye
	
	//test patterns
	load_pipeline(GST_BLANK ,(char *)"videotestsrc pattern=2 ! video/x-raw,width=640,height=480,framerate=(fraction)1/1 ! queue ! glupload ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true");
	load_pipeline(GST_VIDEOTESTSRC ,(char *)"videotestsrc ! video/x-raw,width=640,height=480,framerate=(fraction)30/1 ! queue ! glupload ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true");
	load_pipeline(GST_VIDEOTESTSRC_CUBED ,(char *)"videotestsrc ! video/x-raw,width=640,height=480,framerate=(fraction)30/1 ! queue ! glupload ! glfiltercube ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true");
	
 //camera launch 192.168.1.22 gordon    192.168.1.23 chell
	if(getenv("GORDON")){
		load_pipeline(GST_RPICAMSRC ,(char *)"rpicamsrc preview=0 ! image/jpeg,width=640,height=480,framerate=30/1 ! "
		"queue max-size-time=50000000 leaky=upstream ! jpegparse ! tee name=t "
		"t. ! queue ! rtpjpegpay ! udpsink host=192.168.3.21 port=9000 sync=false "
		"t. ! queue ! jpegdec ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false"
	//	"t. ! queue ! jpegdec ! videorate ! video/x-raw,framerate=10/1 ! videoscale ! video/x-raw,width=400,height=240 ! videoflip method=3 ! jpegenc ! multifilesink location=/var/www/html/tmp/snapshot.jpg sync=false"
  );
	}else if(getenv("CHELL")){
		load_pipeline(GST_RPICAMSRC ,(char *)"rpicamsrc preview=0 ! image/jpeg,width=640,height=480,framerate=30/1 ! "
		"queue max-size-time=50000000 leaky=upstream ! jpegparse ! tee name=t "
		"t. ! queue ! rtpjpegpay ! udpsink host=192.168.3.20 port=9000 sync=false "
		"t. ! queue ! jpegdec ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false"
	//	"t. ! queue ! jpegdec ! videorate ! video/x-raw,framerate=10/1 ! videoscale ! video/x-raw,width=400,height=240 ! videoflip method=3 ! jpegenc ! multifilesink location=/var/www/html/tmp/snapshot.jpg sync=false"
  );
	}
	else {
		printf("SET THE GORDON OR CHELL ENVIRONMENT VARIABLE!");
		exit(1);
	}
	
 
	//normal
	load_pipeline(GST_NORMAL ,(char *)"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	
	//cpu effects drop framerate to 20 from 30 due to cpu usage
	load_pipeline(GST_RADIOACTV    ,(char *)"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! video/x-raw,framerate=20/1 ! videoconvert ! queue ! radioactv     ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_REVTV        ,(char *)"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! video/x-raw,framerate=20/1 ! videoconvert ! queue ! revtv         ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_AGINGTV      ,(char *)"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! video/x-raw,framerate=20/1 ! videoconvert ! queue ! agingtv       ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_DICETV       ,(char *)"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! video/x-raw,framerate=20/1 ! videoconvert ! queue ! dicetv        ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_WARPTV       ,(char *)"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! video/x-raw,framerate=20/1 ! videoconvert ! queue ! warptv        ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_SHAGADELICTV ,(char *)"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! video/x-raw,framerate=20/1 ! videoconvert ! queue ! shagadelictv  ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_VERTIGOTV    ,(char *)"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! video/x-raw,framerate=20/1 ! videoconvert ! queue ! vertigotv     ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_RIPPLETV     ,(char *)"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! video/x-raw,framerate=20/1 ! videoconvert ! queue ! rippletv      ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_EDGETV       ,(char *)"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! video/x-raw,framerate=20/1 ! videoconvert ! queue ! edgetv        ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_STREAKTV     ,(char *)"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! video/x-raw,framerate=20/1 ! videoconvert ! queue ! streaktv      ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_AATV         ,(char *)"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videoconvert                                                                                       ! queue ! aatv rain-mode=0 ! glupload ! glcolorconvert ! glcolorscale ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_CACATV       ,(char *)"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! jpegdec ! queue ! videorate ! videoscale ! videoconvert ! video/x-raw,width=160,height=120,framerate=20/1,format=RGB ! queue ! cacatv           ! glupload ! glcolorconvert ! glcolorscale ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");

	//gl effects 
	load_pipeline(GST_GLCUBE   ,(char *)"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! glfiltercube      ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_GLMIRROR ,(char *)"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_mirror  ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_GLSQUEEZE,(char *)"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_squeeze ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_GLSTRETCH,(char *)"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_stretch ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_GLTUNNEL ,(char *)"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_tunnel  ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_GLTWIRL  ,(char *)"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_twirl   ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_GLBULGE  ,(char *)"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_bulge   ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	load_pipeline(GST_GLHEAT   ,(char *)"udpsrc port=9000 caps=application/x-rtp retrieve-sender-address=false ! rtpjpegdepay ! queue ! jpegdec ! glupload ! glcolorconvert ! gleffects_heat    ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=false");
	
	//audio effects - alsasrc takes a second or so to init, so here a output-selector is used
	//effect order matters since pads on the output selector can't easily be named in advance 
	//audio format must match the movie output stuff, otherwise the I2S Soundcard will get slow and laggy when switching formats!
	load_pipeline(GST_LIBVISUAL_FIRST,(char *)"alsasrc buffer-time=10000 ! audio/x-raw,layout=interleaved,rate=48000,format=S32LE,channels=2 ! queue max-size-time=10000000 leaky=downstream ! audioconvert ! "
	"output-selector name=audioin pad-negotiation-mode=Active "
	"audioin. ! libvisual_jess     ! videosink. "
	"audioin. ! libvisual_infinite ! videosink. "
	"audioin. ! libvisual_jakdaw   ! videosink. "
	"audioin. ! libvisual_oinksie  ! videosink. "
	"audioin. ! goom               ! videosink. "
	"audioin. ! goom2k1            ! videosink. "
	"funnel name=videosink ! video/x-raw,width=320,height=240,framerate=30/1 ! "
	"glupload ! glcolorconvert ! glcolorscale ! video/x-raw(memory:GLMemory),width=640,height=480 ! "
	"glfilterapp name=grabtexture ! fakesink sync=false async=false");
	
	//movie pipeline, has all videos as long long video, chapter start and end times stored in gstvideo.h
	//it doesnt work well to load and unload various input files due to the 
	//audio format must match the visual input stuff, otherwise the I2S Soundcard will get slow and laggy when switching formats! 
	load_pipeline(GST_MOVIE_FIRST ,(char *) "filesrc location=/home/pi/assets/movies/all.mp4 ! qtdemux name=dmux "
	"dmux.video_0 ! queue ! avdec_h264 ! queue ! videoconvert ! "
	"glupload ! glcolorscale ! glcolorconvert ! video/x-raw(memory:GLMemory),width=640,height=480,format=RGBA ! glfilterapp name=grabtexture ! fakesink sync=true async=false "
  //"dmux.audio_0 ! queue ! aacparse ! avdec_aac ! audioconvert ! audio/x-raw,layout=interleaved,rate=48000,format=S32LE,channels=2 ! alsasink sync=true async=false device=dmix");
  "dmux.audio_0 ! fakesink");  //disable sound for testing on the workbench

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


	FontNormal.Load("/home/pi/portal/gstvideo/Consolas.bff");
init_text_background();
	model_board_init();
	
	//start the idle and main loops
	loop = g_main_loop_new (NULL, FALSE);
	//gpointer data = NULL;
	//g_idle_add (idle_loop, data);
	//g_timeout_add (10,idle_loop ,pipeline); 
	g_timeout_add_full ( G_PRIORITY_HIGH,6,idle_loop ,pipeline,NULL); 
	g_main_loop_run (loop); //let gstreamer's GLib event loop take over
	
	glXMakeCurrent(dpy, None, NULL);
	glXDestroyContext(dpy, ctx);
	XDestroyWindow(dpy, win);
	XCloseDisplay(dpy);
	
	return 0;
}
