#include <stdio.h>
#include <gst/gst.h>
#include <gst/gl/gl.h>
#include <gst/gl/egl/gstgldisplay_egl.h>
#include "memory.h"
#include "../common/common.h"

extern struct egl *egl; //needed to access egl context and display
extern struct gun_struct *this_gun;  //needed only to report in finished videos
extern GstPipeline *pipeline[];  //this lets the gstreamer file define how many pipelines we will have

static GstContext *egl_context;
static GstContext *gst_context;
static GMainLoop *loop;

volatile GLuint gstcontext_texture_id;   //used by opengl
volatile bool gstcontext_texture_fresh = false;

//grabs the texture via the glfilterapp element
static gboolean drawCallback(GstElement* object, guint id , guint width ,guint height, gpointer user_data){
	gstcontext_texture_id = id;
	gstcontext_texture_fresh = true;
	return true;
}

gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data)
{
	GMainLoop *loop = (GMainLoop*)data;

	switch (GST_MESSAGE_TYPE (msg))	{
	case GST_MESSAGE_EOS:
		g_print ("End-of-stream\n");  //pausing or nulling a stream wont trigger this.
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
				gst_element_set_context (GST_ELEMENT (msg->src), gst_context);  			
			}
			if (g_strcmp0 (context_type, GST_GL_DISPLAY_CONTEXT_TYPE) == 0) {
				g_print("X11 Display Request Intercepted! %s\n", context_type);			
				gst_element_set_context (GST_ELEMENT (msg->src), egl_context);			
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

void load_pipeline(int i, char * text)
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
	
	gst_element_set_context(GST_ELEMENT (pipeline[i]), gst_context);  			
	gst_element_set_context(GST_ELEMENT (pipeline[i]), egl_context);

    gst_element_set_state (GST_ELEMENT (pipeline[i]), GST_STATE_PLAYING);
	gst_element_set_state (GST_ELEMENT (pipeline[i]), GST_STATE_PAUSED);  //init this differently?  do all plugins like being left paused?
}

void gstshared_init(void){
	/* Initialize GStreamer */
	static char *arg1_gst[]  = {"hud"}; 
	static char *arg2_gst[]  = {"--gst-disable-registry-update"};  //dont rescan the registry to load faster.
	static char *arg3_gst[]  = {"--gst-debug-level=1"};  //dont show debug messages
	static char **argv_gst[] = {arg1_gst,arg2_gst,arg3_gst};
	int argc_gst = sizeof(argv_gst)/sizeof(char *);
	gst_init (&argc_gst, argv_gst);
	
	//get context ready for handing off to elements in the callback
	GstGLDisplay * gl_display = GST_GL_DISPLAY (gst_gl_display_egl_new_with_egl_display (egl->display));	
	egl_context = gst_context_new (GST_GL_DISPLAY_CONTEXT_TYPE, TRUE);
	gst_context_set_gl_display(egl_context, gl_display);
	
	//get gst_context ready for handing off to elements in the callback
	GstGLContext *gl_context = gst_gl_context_new_wrapped ( gl_display, (guintptr) egl->context ,GST_GL_PLATFORM_EGL ,GST_GL_API_GLES2);
	gst_context = gst_context_new ("gst.gl.app_context", TRUE);
	gst_structure_set (gst_context_writable_structure (gst_context), "context", GST_TYPE_GL_CONTEXT, gl_context, NULL);

}
