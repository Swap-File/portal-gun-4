//
// This file handles setting up the shared gstreamer context and loading pipelines into it
//
#include <stdio.h>
#include <stdbool.h>
#include <gst/gst.h>
#include <gst/gl/gl.h>
#include <gst/gl/egl/gstgldisplay_egl.h>
#include <EGL/egl.h>   //EGLDisplay & EGLContext
#include <GLES3/gl31.h> //GLint
#include "effects.h"

static GstContext *egl_context;
static GstContext *gst_context;

//outgoing flag
static volatile bool *video_done_flag = NULL;  //points to shared struct (if set)

//incoming
volatile GLint *texture_id;
volatile bool  *texture_fresh;

//grabs the texture via the glfilterapp element
static gboolean drawCallback(GstElement* object, guint id, guint width,guint height, gpointer user_data) {
    *texture_id = id;
    *texture_fresh = true;
    return true;
}

void gstcontext_set(GstPipeline **pipeline)
{
    gst_element_set_context(GST_ELEMENT (*pipeline), gst_context);
    gst_element_set_context(GST_ELEMENT (*pipeline), egl_context);
}

void gstcontext_handle_bus(GstPipeline **pipeline)
{
	 GstMessage *msg = gst_bus_pop_filtered (GST_ELEMENT_BUS(*pipeline), GST_MESSAGE_EOS);
	 
	 if (msg != NULL){
		g_print ("End-of-stream\n");  //pausing or nulling a stream wont trigger this.
        if (video_done_flag != NULL) *video_done_flag = true;
	 }
}

void gstcontext_load_pipeline(int index, GstPipeline **pipeline, GstState state, char * text)
{
    printf("Loading %s...\n",effectnames[index]);

    *pipeline = GST_PIPELINE(gst_parse_launch(text, NULL));

    //set the glfilterapp callback that will capture the textures
    //do this AFTER attaching the bus handler so context can be set
    GstElement *grabtexture = gst_bin_get_by_name(GST_BIN (*pipeline), "grabtexture");
    g_signal_connect(grabtexture, "client-draw",  G_CALLBACK (drawCallback), NULL);
    gst_object_unref(grabtexture);

    gstcontext_set(pipeline);
    gst_element_set_state (GST_ELEMENT (*pipeline), state);
}

void gstcontext_init(EGLDisplay display, EGLContext context,volatile GLint *texture_id_p,volatile bool *texture_fresh_p,volatile bool *video_done_flag_p) {
    video_done_flag = video_done_flag_p;
    texture_id = texture_id_p;
    texture_fresh = texture_fresh_p;

    /* Initialize GStreamer */
    static char *arg1_gst[]  = {"gstcontext"};
    static char *arg2_gst[]  = {"--gst-disable-registry-update"};  //dont rescan the registry to load faster.
    static char *arg3_gst[]  = {"--gst-debug-level=1"};  //dont show debug messages
    static char **argv_gst[] = {arg1_gst,arg2_gst,arg3_gst};
    int argc_gst = sizeof(argv_gst)/sizeof(char *);
    gst_init (&argc_gst, argv_gst);

    //get context ready for handing off to elements in the callback
    GstGLDisplay * gl_display = GST_GL_DISPLAY (gst_gl_display_egl_new_with_egl_display (display));
    egl_context = gst_context_new (GST_GL_DISPLAY_CONTEXT_TYPE, TRUE);
    gst_context_set_gl_display(egl_context, gl_display);

    //get gst_context ready for handing off to elements in the callback
    GstGLContext *gl_context = gst_gl_context_new_wrapped ( gl_display, (guintptr) context,GST_GL_PLATFORM_EGL,GST_GL_API_GLES2);
    gst_context = gst_context_new ("gst.gl.app_context", TRUE);
    gst_structure_set (gst_context_writable_structure (gst_context), "context", GST_TYPE_GL_CONTEXT, gl_context, NULL);

}
