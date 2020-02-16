#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include "../common/memory.h"
#include "../common/opengl.h"
#include "../common/esUtil.h"
#include "../common/gstcontext.h"
#include "../common/effects.h"

#include "projector_logic.h"
#include "projector_scene.h"
#include "png.h"

struct gun_struct *this_gun;
static volatile GLint gstcontext_texture_id; //passed to gstcontext
static volatile bool gstcontext_texture_fresh; //passed to gstcontext

static struct egl egl;

static GLuint basic_program,basic_u_mvpMatrix,basic_in_Position,basic_in_TexCoord,basic_u_Texture;
static GLuint portal_program,portal_in_TexCoord,portal_in_Position,portal_u_blue,portal_u_time,portal_u_size,portal_u_shutter;
static GLuint ripple_program,ripple_in_TexCoord,ripple_in_Position,ripple_u_time,ripple_u_Alpha,ripple_u_blue,ripple_u_size;
static GLuint points_program,points_vertex,points_color,points_sprite;

static GLuint vbo,points_vbo;

#define VIDEO_WIDTH 1280
#define VIDEO_HEIGHT 720


static GLfloat vVertices2[3 * 3000];
static GLfloat pointcolors[4 * 3000];

struct Particles {
	GLfloat x,y,size;
	float r;
	float angle;
	float angle_velocity;
	float r_velocity;
};  

struct Particles particle[1000];

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
		usleep(10);
		if (millis() - start_time > 20){
			printf("Frameskip\n");
			break;
		}
	}
	gstcontext_texture_fresh = false;
	
	static uint8_t portal_state_displayed = PORTAL_CLOSED;
	#define ZOOM_MIN 20.0
	#define ZOOM_MAX 0.44
	static float zoom_displayed = ZOOM_MIN;
	static float shimmer_alpha = 1.0;	
	
	uint8_t portal_state_requested = this_gun->portal_state;
	
	//have gstreamer update based on last cycles values
	//this ensures the portal will close before gstreamer changes
	//this is pass by value on purpose, it prevents gst_state from changing during execution
	projector_logic_update(this_gun->gst_state,portal_state_displayed);  
	

	//calculate the delta between frames
	const int speed = 10000;
	static uint32_t last_frame_time = 0;
	uint32_t this_frame_time = micros(); //use micros for better resolution
	uint32_t time_delta = this_frame_time - last_frame_time;
	float delta = (float)time_delta / speed; //must cast to float! delta should be 3.33 per frame @ 30 fps
	last_frame_time = this_frame_time;
	

	//if we are displaying one color and a request comes in for the other color, reset all and spend one frame closed
	//immediately close the portal if requested, unzoom, and unfade.
	if(((portal_state_displayed & PORTAL_BLUE_BIT)   != 0 && (portal_state_requested & PORTAL_ORANGE_BIT) != 0) || 
			((portal_state_displayed & PORTAL_ORANGE_BIT) != 0 && (portal_state_requested & PORTAL_BLUE_BIT)   != 0) || 
			(portal_state_requested == PORTAL_CLOSED)){
		portal_state_displayed = PORTAL_CLOSED;
		zoom_displayed = ZOOM_MIN;
	}
	
	//immediately close portals if requested
	if((portal_state_requested & PORTAL_CLOSED_BIT) != 0){
		shimmer_alpha = 1.0;
	}
	
	//zoom on request
	if(portal_state_requested != PORTAL_CLOSED && zoom_displayed > ZOOM_MAX){
		zoom_displayed -= delta / 10;  //This sets zoom speed
		if (zoom_displayed < ZOOM_MAX) zoom_displayed = ZOOM_MAX;
		//printf("zooming %f \n",zoom_displayed);
		if ((portal_state_requested & PORTAL_ORANGE_BIT) != 0) 
		portal_state_displayed = PORTAL_CLOSED_ORANGE;
		else if ((portal_state_requested & PORTAL_BLUE_BIT) != 0) 
		portal_state_displayed = PORTAL_CLOSED_BLUE;	
	}
	//if fully zoomed, fade shimmer on request
	if((portal_state_requested & PORTAL_OPEN_BIT) != 0){
		if (zoom_displayed <= ZOOM_MAX && shimmer_alpha != 0.0){
			shimmer_alpha -= delta / 330;  //This sets fade speed
			if (shimmer_alpha < 0.0) shimmer_alpha = 0.0;
			//printf("shimmer_alpha %f \n",shimmer_alpha);
			portal_state_displayed = portal_state_requested;
		}
	}

	uint32_t render_start_time = micros();
	
	glClearColor(0.3, 0.3, 0.3, 0.3);
	
	glClear(GL_COLOR_BUFFER_BIT );  
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	ESMatrix modelviewprojection;
	esMatrixLoadIdentity(&modelviewprojection);
	esOrtho(&modelviewprojection, -1366/2, 1366/2,-768/2, 768/2, -1,1);

	//this is the polygon that displays the gstreamer texture
	glUseProgram(basic_program);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,gstcontext_texture_id);
	glUniform1i(basic_u_Texture, 0); /* '0' refers to texture unit 0. */
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glEnableVertexAttribArray(basic_in_Position);
	glVertexAttribPointer(basic_in_Position, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)0);
	glEnableVertexAttribArray(basic_in_TexCoord);
	glVertexAttribPointer(basic_in_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)sizeof(vVertices));
	glUniformMatrix4fv(basic_u_mvpMatrix, 1, GL_FALSE, &modelviewprojection.m[0][0]);
	
	/* Draw & Disable */
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glDisableVertexAttribArray(basic_in_Position);
	glDisableVertexAttribArray(basic_in_TexCoord);
	
	
	/* Portal Shimmer Shader Setup */
	glUseProgram(ripple_program);
	glEnableVertexAttribArray(ripple_in_Position);
	glEnableVertexAttribArray(ripple_in_TexCoord);
	glVertexAttribPointer(ripple_in_Position, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)0);
	glVertexAttribPointer(ripple_in_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)sizeof(vVertices));
	
	/* Time */ 
	glUniform1f(ripple_u_time,(float)millis()/7000);
	
	/* Transparency */
	glUniform1f(ripple_u_Alpha,shimmer_alpha);

	/* sSize */
	glUniform1f(ripple_u_size,zoom_displayed);
	
	/* Color */	
	if((portal_state_displayed & PORTAL_BLUE_BIT) != 0)	glUniform1f(ripple_u_blue,TRUE);
	else												glUniform1f(ripple_u_blue,FALSE);
	
	/* Draw & Disable */	
	glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
	glDisableVertexAttribArray(ripple_in_Position);
	glDisableVertexAttribArray(ripple_in_TexCoord);

	
	
	glUseProgram(points_program);
	glBindTexture(GL_TEXTURE_2D,points_sprite);
	glUniform1i(points_sprite, 0); /* '0' refers to texture unit 0. */
	glBindBuffer(GL_ARRAY_BUFFER, points_vbo);
	
	glEnableVertexAttribArray(points_vertex);
	glEnableVertexAttribArray(points_color);
	
	for(int i = 0; i < 1000; i++){
		
	   //move current point to be tail
		vVertices2[i*3+6000] =  particle[i].r * cos((float) ((particle[i].angle-.5) / 360.0) * M_PI*2);
		vVertices2[i*3+6001] = particle[i].r * sin((float) ((particle[i].angle-.5) / 360.0) * M_PI*2);
		vVertices2[i*3+6002] = 2.0;
		
		//make current point background flare
		vVertices2[i*3+0] = particle[i].r * cos((float) (particle[i].angle / 360.0) * M_PI*2);
		vVertices2[i*3+1] = particle[i].r * sin((float) (particle[i].angle / 360.0) * M_PI*2);
		vVertices2[i*3+2] = 6.0;
		
		//make point core
		vVertices2[i*3+3000] = vVertices2[i*3+0];
		vVertices2[i*3+3001] = vVertices2[i*3+1];
		vVertices2[i*3+3002] = 2.0;
		
	
		particle[i].angle +=particle[i].angle_velocity;
		particle[i].r += particle[i].r_velocity;
		
		if (particle[i].r < .8 || particle[i].r > 1.0){
			particle[i].r_velocity = -.001 + 0.002* (float)(rand() % 1000) /1000;
		}

	}
	
	
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vVertices2), &vVertices2[0]);

	glVertexAttribPointer(points_vertex, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)0);
	glVertexAttribPointer(points_color, 4, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)sizeof(vVertices2));
	
	glBufferSubData(GL_ARRAY_BUFFER,  sizeof(vVertices2), sizeof(pointcolors), &pointcolors[0]);
	glDrawArrays(GL_POINTS, 0, 3000);
	
	glDisableVertexAttribArray(points_vertex);
	glDisableVertexAttribArray(points_color);

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	/* Portal Rim Shader Setup */
	glUseProgram(portal_program);
	glEnableVertexAttribArray(portal_in_Position);
	glEnableVertexAttribArray(portal_in_TexCoord);
	glVertexAttribPointer(portal_in_Position, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)0);
	glVertexAttribPointer(portal_in_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)sizeof(vVertices));
	
	/* Time */
	glUniform1f(portal_u_time,(float)millis()/1000);
	
	/* Color */
	if((portal_state_displayed & PORTAL_BLUE_BIT) != 0)	glUniform1f(portal_u_blue,TRUE);
	else 												glUniform1f(portal_u_blue,FALSE);
	
	/* Safety Shutter - Screen Blanking */
	if (portal_state_displayed == PORTAL_CLOSED)	glUniform1f(portal_u_shutter,TRUE);
	else											glUniform1f(portal_u_shutter,FALSE);  
	
	/* Size */
	glUniform1f(portal_u_size,zoom_displayed);  
	
	/* Draw & Disable */
	glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);
	glDisableVertexAttribArray(portal_in_Position);
	glDisableVertexAttribArray(portal_in_TexCoord);
	
	


	

	/* FPS Counter */
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
	
	//upload data
	glGenBuffers(1, &vbo);
	
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices) + sizeof(vTexCoords), 0, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vVertices), &vVertices[0]);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(vVertices), sizeof(vTexCoords), &vTexCoords[0]);

	
	points_program = create_program_from_disk("points.vert","points.frag");
	link_program(points_program);
	points_vertex = glGetAttribLocation(points_program, "vertex");	
	points_color = glGetAttribLocation(points_program, "in_color");
	points_sprite = glGetUniformLocation(points_program, "u_Texture");
	

	for(int i=0; i < 1000; i++){
		particle[i].angle = ((float)(rand() % 36000))/100;
		particle[i].r = .75 + .1* (float)(rand() % 1000) /1000;
		particle[i].angle_velocity = 1.0 + .2* (float)(rand() % 1000) /1000;
		particle[i].r_velocity = -.001 + 0.002* (float)(rand() % 1000) /1000;
		
	}
	
	for(int i =0; i < 1000; i++){
		pointcolors[i*4+0] = 0.25;
		pointcolors[i*4+1] = 0.35;
		pointcolors[i*4+2] = 1.0;
		pointcolors[i*4+3] = 1.0;
	}
	for(int i = 1000; i < 2000; i++){
		pointcolors[i*4+0] = 0.4;
		pointcolors[i*4+1] = .8;
		pointcolors[i*4+2] = 1.0;
		pointcolors[i*4+3] = 1.0;
	}
	for(int i = 2000; i < 3000; i++){
		pointcolors[i*4+0] = 0.5;
		pointcolors[i*4+1] = 0.5;
		pointcolors[i*4+2] = 0.5;
		pointcolors[i*4+3] = 1.0;
	}
	glGenBuffers(1, &points_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, points_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices2) + sizeof(pointcolors), 0, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vVertices2), &vVertices2[0]);
	glBufferSubData(GL_ARRAY_BUFFER,  sizeof(vVertices2), sizeof(pointcolors), &pointcolors[0]);
	
	//fire up gstreamer
	gstcontext_init(egl.display, egl.context, &gstcontext_texture_id, &gstcontext_texture_fresh, &this_gun->video_done);
	projector_logic_init(&this_gun->video_done);
	points_sprite = png_load("circle64.png", NULL, NULL);
	
	egl.draw = projector_scene_draw;

	this_gun->projector_loaded = true;

	return &egl;
}
