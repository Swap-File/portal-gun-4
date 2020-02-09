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

struct gun_struct *this_gun;
static volatile GLint gstcontext_texture_id; //passed to gstcontext
static volatile bool gstcontext_texture_fresh; //passed to gstcontext

static struct egl egl;

static GLuint basic_program,basic_u_mvpMatrix,basic_in_Position,basic_in_TexCoord,basic_u_Texture,basic_u_Alpha;
static GLuint portal_program,portal_in_TexCoord,portal_in_Position,portal_u_blue,portal_u_time,portal_u_size,portal_u_shutter;
static GLuint ripple_program,ripple_in_TexCoord,ripple_in_Position,ripple_u_time,ripple_u_Alpha,ripple_u_blue,ripple_u_size;
		
/* uniform handles: */
static GLuint vbo;
static GLuint positionsoffset, texcoordsoffset;

#define VIDEO_WIDTH 1280
#define VIDEO_HEIGHT 720

static const GLfloat vVertices[] = {
		// video
		-VIDEO_WIDTH/2, -VIDEO_HEIGHT/2, 0.0f,
		+VIDEO_WIDTH/2, -VIDEO_HEIGHT/2, 0.0f,
		-VIDEO_WIDTH/2, +VIDEO_HEIGHT/2, 0.0f,
		+VIDEO_WIDTH/2, +VIDEO_HEIGHT/2, 0.0f,
		// shimmer 1
		-1.0f, -1.0f, 0.0f,
		+1.0f, -1.0f, 0.0f,
		-1.0f, +1.0f, 0.0f,
		+1.0f, +1.0f, 0.0f,

		// portal
	-1.0f, -1.0f, 0.0f,
		+1.0f, -1.0f, 0.0f,
		-1.0f, +1.0f, 0.0f,
		+1.0f, +1.0f, 0.0f,
};

static GLfloat vTexCoords[] = {
		// video
		1.0f, 1.0f,
		0.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,
		// shimmer
		0.5f, 0.5f,
		-0.5f, 0.5f,
		0.5f, -0.5f,
		-0.5f, -0.5f,
		// portal
		1.0f, 1.0f,
		0.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,
};

static void projector_scene_draw(unsigned i,char *debug_msg)
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

	glUseProgram(basic_program);
	//glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,gstcontext_texture_id);
	glUniform1i(basic_u_Texture, 0); /* '0' refers to texture unit 0. */
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glEnableVertexAttribArray(basic_in_Position);
	glVertexAttribPointer(basic_in_Position, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)positionsoffset);
	glEnableVertexAttribArray(basic_in_TexCoord);
	glVertexAttribPointer(basic_in_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)texcoordsoffset);

	glUniform1f(basic_u_Alpha, 1.0);
	glUniformMatrix4fv(basic_u_mvpMatrix, 1, GL_FALSE, &modelviewprojection.m[0][0]);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	
	

	glDisableVertexAttribArray(basic_in_Position);
	glDisableVertexAttribArray(basic_in_TexCoord);
	

	static float portal_u_sizevar = 0.44;
	
	glEnableVertexAttribArray(ripple_in_Position);
	glEnableVertexAttribArray(ripple_in_TexCoord);
	glUseProgram(ripple_program);
	glVertexAttribPointer(ripple_in_Position, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)positionsoffset);
	glVertexAttribPointer(ripple_in_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)texcoordsoffset);
	glUniform1f(ripple_u_time,(float)millis()/7000);

	//static float transparent = 0.5;
	//transparent += 0.001;
	//if (transparent > 1.0)
	//	glUniform1f(ripple_u_Alpha,1.0);
	//else	
	glUniform1f(ripple_u_Alpha,1.0);
		
	//if (transparent > 2.0) transparent= 0.0;
	
   glUniform1f(ripple_u_size,portal_u_sizevar);

    glUniform1f(ripple_u_blue,true);
	glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
	glDisableVertexAttribArray(ripple_in_Position);
	glDisableVertexAttribArray(ripple_in_TexCoord);
	
		
	glEnableVertexAttribArray(portal_in_Position);
	glEnableVertexAttribArray(portal_in_TexCoord);
	glUseProgram(portal_program);
	glVertexAttribPointer(portal_in_Position, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)positionsoffset);
	glVertexAttribPointer(portal_in_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)texcoordsoffset);
	glUniform1f(portal_u_time,(float)millis()/1000);
	glUniform1f(portal_u_blue,FALSE);
	glUniform1f(portal_u_shutter,FALSE);
	
	glUniform1f(portal_u_size,portal_u_sizevar);
	//portal_u_sizevar += 0.01;
	if (portal_u_sizevar > 1 ) portal_u_sizevar = 0.44;
	
	glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);
	glDisableVertexAttribArray(portal_in_Position);
	glDisableVertexAttribArray(portal_in_TexCoord);


	
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

	basic_program = create_program_from_disk("../common/basic.vert", "../common/basic.frag");
	link_program(basic_program);
	basic_u_mvpMatrix = glGetUniformLocation(basic_program, "u_mvpMatrix");
	basic_in_Position = glGetAttribLocation(basic_program, "in_Position");
	basic_in_TexCoord = glGetAttribLocation(basic_program, "in_TexCoord");
	basic_u_Texture = glGetAttribLocation(basic_program, "u_Texture");
	basic_u_Alpha = glGetUniformLocation(basic_program, "u_Alpha");
	
	portal_program = create_program_from_disk("portal.vert","portal.frag");
	link_program(portal_program);
	portal_in_TexCoord = glGetAttribLocation(portal_program, "in_TexCoord");
	portal_in_Position = glGetAttribLocation(portal_program, "in_Position");
	portal_u_shutter = glGetUniformLocation(portal_program, "u_shutter");
	portal_u_blue = glGetUniformLocation(portal_program, "u_blue");
	portal_u_time = glGetUniformLocation(portal_program, "u_time");
	portal_u_size = glGetUniformLocation(portal_program, "u_size");
	
	ripple_program = create_program_from_disk("ripple.vert","ripple.frag");
	link_program(ripple_program);
	ripple_in_TexCoord = glGetAttribLocation(ripple_program, "in_TexCoord");
	ripple_in_Position = glGetAttribLocation(ripple_program, "in_Position");
	ripple_u_time = glGetUniformLocation(ripple_program, "u_time");
	ripple_u_Alpha = glGetUniformLocation(ripple_program, "u_Alpha");
	ripple_u_blue = glGetUniformLocation(ripple_program, "u_blue");
	ripple_u_size = glGetUniformLocation(ripple_program, "u_size");
	
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

	egl.draw = projector_scene_draw;

	this_gun->projector_loaded = true;

	return &egl;
}
