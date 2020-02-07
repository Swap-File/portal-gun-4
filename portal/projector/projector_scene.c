#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "../common/memory.h"
#include "../common/opengl.h"
#include "../common/esUtil.h"
#include "../common/gstcontext.h"

#include "projector_logic.h"
#include "projector_scene.h"
#include "png.h"

struct gun_struct *this_gun;
static volatile GLint gstcontext_texture_id; //passed to gstcontext
static volatile bool gstcontext_texture_fresh; //passed to gstcontext

//textures
static GLuint orange_1,orange_0,blue_0,blue_1;
static struct egl egl;
static GLfloat aspect;
static GLuint program,portal_program;

GLint portal_uv,portal_position,portal_modelViewMatrix,portal_projectionMatrix,portal_iTime;

/* uniform handles: */
static GLint  modelviewprojectionmatrix;
static GLuint vbo;
static GLuint positionsoffset, texcoordsoffset;

#define VIDEO_WIDTH 1280
#define VIDEO_HEIGHT 720

static const GLfloat vVertices[] = {
		// the video
		-VIDEO_WIDTH/2, -VIDEO_HEIGHT/2, 0.0f,
		+VIDEO_WIDTH/2, -VIDEO_HEIGHT/2, 0.0f,
		-VIDEO_WIDTH/2, +VIDEO_HEIGHT/2, 0.0f,
		+VIDEO_WIDTH/2, +VIDEO_HEIGHT/2, 0.0f,
		// front
		-1.0f, -1.0f, 0.0f,
		+1.0f, -1.0f, 0.0f,
		-1.0f, +1.0f, 0.0f,
		+1.0f, +1.0f, 0.0f,
		// front

};


GLfloat vTexCoords[] = {
		//front
		1.0f, 1.0f,
		0.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,
		//back
		1.0f, 1.0f,
		0.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,
		//right
};

static void scene_draw(unsigned i,char *debug_msg)
{
	if (debug_msg[0] != '\0'){
		int temp[2];
		int result = sscanf(debug_msg,"%d %d", &temp[0],&temp[1]);
		if (result == 2){
			printf("\nDebug Message Parsed: Setting gst_state: %d and portal_state: %d\n",temp[0],temp[1]);
			this_gun->gst_state = temp[0];
			this_gun->portal_state = temp[1];
		}
	}

	uint32_t start_time = millis();
	while (gstcontext_texture_fresh == false){
		usleep(100);
		if (millis() - start_time > 20){
			printf("Frameskip\n");
			break;
		}
	}
	gstcontext_texture_fresh = false;
	projector_logic_update(this_gun->gst_state,this_gun->portal_state);
	uint32_t render_start_time = micros();

	const int speed = 10000;
	static uint32_t last_frame_time = 0;
	uint32_t this_frame_time = micros(); //use micros for better resolution
	uint32_t time_delta = this_frame_time - last_frame_time;
	float delta = (float)time_delta / speed; //must cast to float!
	last_frame_time = this_frame_time;

	static float rotation = 0;
	rotation += delta * 0.3; //we only want to cut the delta speed, not increase, or jitter will happen

	glClearColor(0.3, 0.3, 0.3, 0.3);
	glClear(GL_COLOR_BUFFER_BIT);
	// Enable blending, necessary for our alpha texture
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


	ESMatrix modelviewprojection;
	esMatrixLoadIdentity(&modelviewprojection);
    esOrtho(&modelviewprojection, -1366/2, 1366/2,-768/2, 768/2, -1,1);

	
	glUseProgram(program);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture( GL_TEXTURE_2D, gstcontext_texture_id);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)positionsoffset);
	//glEnableVertexAttribArray(0);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)texcoordsoffset);
	//glEnableVertexAttribArray(2);

	glUniformMatrix4fv(modelviewprojectionmatrix, 1, GL_FALSE, &modelviewprojection.m[0][0]);

	//glUniform1i(texture, 0); /* '0' refers to texture unit 0. */
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	//glDisableVertexAttribArray(0);
	//glDisableVertexAttribArray(1);
	//glDisableVertexAttribArray(2);

	ESMatrix modelview;
	esMatrixLoadIdentity(&modelview);
	esTranslate(&modelview, 0.0f, 0.0f, -8.0f);
	//const float scale = 4.8f;
	//esScale(&modelview,scale * 1366/768,scale,1.0f);
	//esRotate(&modelview, rotation , 0.0f, 0.0f, 1.0f);
	//esScale(&modelview,100,100,1);
	ESMatrix projection;
	esMatrixLoadIdentity(&projection);
	esFrustum(&projection, -2.8f, +2.8f, -2.8f * aspect, +2.8f * aspect, 6.0f, 10.0f);
	esMatrixLoadIdentity(&modelviewprojection);
	esMatrixMultiply(&modelviewprojection, &modelview, &projection);


	glEnableVertexAttribArray(portal_position);
	glEnableVertexAttribArray(portal_uv);
	glUseProgram(portal_program);
	glVertexAttribPointer(portal_position, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)positionsoffset);
	glVertexAttribPointer(portal_uv, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)texcoordsoffset);
	glUniform1f(portal_iTime,(float)millis()/1000);
	//portal_iTime = glGetUniformLocation(portal_program, "iTime");
	//portal_uv = glGetAttribLocation(portal_program, "in_TexCoord");
	//portal_position = glGetAttribLocation(portal_program, "in_position");
	//portal_modelViewMatrix = glGetUniformLocation(portal_program, "modelviewprojectionMatrix");

	glUniformMatrix4fv(portal_modelViewMatrix, 1, GL_FALSE, &modelview.m[0][0]);

	glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);

	//glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);
	//glDrawArrays(GL_TRIANGLE_STRIP, 12, 4);
	//glDrawArrays(GL_TRIANGLE_STRIP, 16, 4);
	//glDrawArrays(GL_TRIANGLE_STRIP, 20, 4);


	/* FPS counter */
	glFinish();
	fps_counter("Projector:",render_start_time);
}


const struct egl * scene_init(const struct gbm *gbm, int samples)
{

	shared_init(&this_gun,false);
	this_gun->gst_state = 1;

	int ret = init_egl(&egl, gbm, samples);
	if (ret)
		return NULL;

	if (egl_check(&egl, eglCreateImageKHR) ||
	    egl_check(&egl, glEGLImageTargetTexture2DOES) ||
	    egl_check(&egl, eglDestroyImageKHR))
		return NULL;

	aspect = (GLfloat)(gbm->height) / (GLfloat)(gbm->width);

	program = create_program_from_disk("../common/basic.vert", "../common/basic.frag");
	glBindAttribLocation(program, 0, "in_position");
	link_program(program);
	modelviewprojectionmatrix = glGetUniformLocation(program, "modelviewprojectionMatrix");
	

	portal_program = create_program_from_disk("portal.vert","portal.frag");
	link_program(portal_program);
	portal_iTime = glGetUniformLocation(portal_program, "iTime");
	portal_uv = glGetAttribLocation(portal_program, "in_TexCoord");
	portal_position = glGetAttribLocation(portal_program, "in_position");
	portal_modelViewMatrix = glGetUniformLocation(portal_program, "modelviewprojectionMatrix");


	positionsoffset = 0;
	texcoordsoffset = sizeof(vVertices);

	//upload data
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices) + sizeof(vTexCoords), 0, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, positionsoffset, sizeof(vVertices), &vVertices[0]);
	glBufferSubData(GL_ARRAY_BUFFER, texcoordsoffset, sizeof(vTexCoords), &vTexCoords[0]);

	//fire up gstreamer
	gstcontext_init(egl.display, egl.context, &gstcontext_texture_id, &gstcontext_texture_fresh, &this_gun->video_done);
	projector_logic_init(&this_gun->video_done);

	//load textures
	orange_0 = png_load("/home/pi/assets/orange_0.png", NULL, NULL);
	orange_1 = png_load("/home/pi/assets/orange_1.png", NULL, NULL);
	blue_0 = png_load("/home/pi/assets/blue_0.png", NULL, NULL);
	blue_1 = png_load("/home/pi/assets/blue_1.png", NULL, NULL);

	if (orange_0 == 0 || orange_1 == 0 || blue_0 == 0 || blue_1 == 0) {
		printf("Loading textures failed.\n");
		exit(1);
	}

	egl.draw = scene_draw;

	this_gun->projector_loaded = true;

	return &egl;
}
