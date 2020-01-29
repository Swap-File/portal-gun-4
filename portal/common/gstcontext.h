#ifndef _GSTCONTEXT_H
#define _GSTCONTEXT_H

#include <stdbool.h>
#include <gst/gst.h> //GstPipeline
#include <EGL/egl.h>   //EGLDisplay & EGLContext
#include <GLES3/gl3.h> //GLint

void gstcontext_load_pipeline(GstPipeline *pipeline, char * text);
void gstcontext_init(EGLDisplay display, EGLContext context,volatile GLint *temp_texture_id ,volatile bool *temp_texture_fresh,volatile bool *temp_video_done_flag);

#endif