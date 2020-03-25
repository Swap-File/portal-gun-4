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
static GLuint puddle_program,puddle_in_TexCoord,puddle_in_Position,puddle_u_time,puddle_u_Alpha,puddle_u_blue,puddle_u_size;
static GLuint particles_program,particles_vertex,particles_color,particle_sprite;

#define VIDEO_WIDTH 1280
#define VIDEO_HEIGHT 720

 
#define NUM_PARTICLES 1000

struct Particle {
	GLfloat x,y,size;
	float r;
	float previous_offset;
	float angle;
	float angle_velocity;
	float r_velocity;
};  

static struct Particle particles[NUM_PARTICLES];

static GLuint particles_vbo; // for particles_vertices & particles_colors
static GLfloat particles_vertices[3 * 3 * NUM_PARTICLES];
static GLfloat particles_colors_blue[4 * 3 * NUM_PARTICLES];
static GLfloat particles_colors_orange[4 * 3 * NUM_PARTICLES];

static GLuint vbo;  // for vVertices & vTexCoords

static const GLfloat vVertices[] = {
	// video
	-VIDEO_WIDTH/2, -VIDEO_HEIGHT/2, 0.0f,
	+VIDEO_WIDTH/2, -VIDEO_HEIGHT/2, 0.0f,
	-VIDEO_WIDTH/2, +VIDEO_HEIGHT/2, 0.0f,
	+VIDEO_WIDTH/2, +VIDEO_HEIGHT/2, 0.0f,
	// puddle
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
	// puddle
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
		int temp[3];
		int result = sscanf(debug_msg,"%d %d %d", &temp[0],&temp[1],&temp[2]);
		if (result == 2){
			printf("\nDebug Message Parsed: Setting gst_state: %d and portal_state: %d\n",temp[0],temp[1]);
			this_gun->gst_state = temp[0];
			this_gun->portal_state = temp[1];
		}
	}
	bool frameskip = false;
	uint32_t start_time = millis();
	while (gstcontext_texture_fresh == false){
		usleep(10);
		if (millis() - start_time > 2){
			frameskip = true;
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
		zoom_displayed -= delta / 2.0;  //This sets zoom speed
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
	glUseProgram(puddle_program);
	glEnableVertexAttribArray(puddle_in_Position);
	glEnableVertexAttribArray(puddle_in_TexCoord);
	glVertexAttribPointer(puddle_in_Position, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)0);
	glVertexAttribPointer(puddle_in_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)sizeof(vVertices));
	
	/* Time */ 
	glUniform1f(puddle_u_time,(float)millis()/7000);
	
	/* Transparency */
	glUniform1f(puddle_u_Alpha,shimmer_alpha);

	/* sSize */
	glUniform1f(puddle_u_size,zoom_displayed);
	
	/* Color */	
	if((portal_state_displayed & PORTAL_BLUE_BIT) != 0)	glUniform1f(puddle_u_blue,TRUE);
	else												glUniform1f(puddle_u_blue,FALSE);
	
	/* Draw & Disable */	
	glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
	glDisableVertexAttribArray(puddle_in_Position);
	glDisableVertexAttribArray(puddle_in_TexCoord);

	
	/* Portal Particle Shader Setup */
	glBindBuffer(GL_ARRAY_BUFFER, particles_vbo);  //switch to points vbo
	glUseProgram(particles_program);
	glBindTexture(GL_TEXTURE_2D,particle_sprite);
	glUniform1i(particle_sprite, 0); /* '0' refers to texture unit 0. */
	
	glEnableVertexAttribArray(particles_vertex);
	glEnableVertexAttribArray(particles_color);
	
	for(int i = 0; i < NUM_PARTICLES; i++){
		
		/* Motion addition */  
		float proposed_offset = MIN2(this_gun->particle_offset[(int)(720.0 * particles[i].angle/360.0 + 720.0) % 720]/6, .05); 
		
		if (proposed_offset > particles[i].previous_offset){
			proposed_offset = MIN2(proposed_offset, particles[i].previous_offset + 0.008); //set max expansion rate
		}else if (proposed_offset < particles[i].previous_offset){
			proposed_offset = MAX2(proposed_offset, particles[i].previous_offset - 0.008); //set max contraction rate
		}
		particles[i].previous_offset =	proposed_offset;
		
		float radius = particles[i].r - proposed_offset; 

		/* Background Ember */  		
		particles_vertices[i*3+0] = radius * cos((float)(particles[i].angle / 360.0) * M_PI*2);
		particles_vertices[i*3+1] = radius * sin((float)(particles[i].angle / 360.0) * M_PI*2);
		particles_vertices[i*3+2] = 6.0;
		
		/* Point Core */
		particles_vertices[i*3+3*NUM_PARTICLES+0] = particles_vertices[i*3+0];
		particles_vertices[i*3+3*NUM_PARTICLES+1] = particles_vertices[i*3+1];
		particles_vertices[i*3+3*NUM_PARTICLES+2] = 2.0;
		
		/* Point Tail */
		particles_vertices[i*3+6*NUM_PARTICLES+0] = radius * cos((float)((particles[i].angle-.2) / 360.0) * M_PI*2);
		particles_vertices[i*3+6*NUM_PARTICLES+1] = radius * sin((float)((particles[i].angle-.2) / 360.0) * M_PI*2);
		particles_vertices[i*3+6*NUM_PARTICLES+2] = 2.0;
		
		particles[i].angle += particles[i].angle_velocity;
		particles[i].r += particles[i].r_velocity;
		
		if (particles[i].r < .8)	particles[i].r_velocity = 0.001 * (float)(rand() % 1000)/1000;
		if (particles[i].r > 1.0)	particles[i].r_velocity = -.001 * (float)(rand() % 1000)/1000;
	}
	
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(particles_vertices), &particles_vertices[0]);
	glVertexAttribPointer(particles_vertex, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)0);
	
	/* Color */	
	if((portal_state_displayed & PORTAL_ORANGE_BIT) != 0){ 
		glVertexAttribPointer(particles_color, 4, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)sizeof(particles_vertices));
	} else { 
		glVertexAttribPointer(particles_color, 4, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)(sizeof(particles_vertices)+sizeof(particles_colors_orange)));
	}
	
	/* Draw & Disable */	
	glDrawArrays(GL_POINTS, 0, NUM_PARTICLES*3);
	glDisableVertexAttribArray(particles_vertex);
	glDisableVertexAttribArray(particles_color);

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
	fps_counter("Projector: ",render_start_time,frameskip);
}


const struct egl * scene_init(const struct gbm *gbm, int samples)
{
	shared_init(&this_gun,false);

	int ret = init_egl(&egl, gbm, samples);
	if (ret)
	return NULL;

	if (egl_check(&egl, eglCreateImageKHR) ||
			egl_check(&egl, glEGLImageTargetTexture2DOES) ||
			egl_check(&egl, eglDestroyImageKHR))
	return NULL;

	basic_program = create_program_from_disk("/home/pi/portal/common/basic.vert", "/home/pi/portal/common/basic.frag");
	link_program(basic_program);
	basic_u_mvpMatrix = glGetUniformLocation(basic_program, "u_mvpMatrix");
	basic_in_Position = glGetAttribLocation(basic_program, "in_Position");
	basic_in_TexCoord = glGetAttribLocation(basic_program, "in_TexCoord");
	basic_u_Texture = glGetAttribLocation(basic_program, "u_Texture");
	
	portal_program = create_program_from_disk("/home/pi/portal/projector/portal.vert","/home/pi/portal/projector/portal.frag");
	link_program(portal_program);
	portal_in_TexCoord = glGetAttribLocation(portal_program, "in_TexCoord");
	portal_in_Position = glGetAttribLocation(portal_program, "in_Position");
	portal_u_shutter = glGetUniformLocation(portal_program, "u_shutter");
	portal_u_blue = glGetUniformLocation(portal_program, "u_blue");
	portal_u_time = glGetUniformLocation(portal_program, "u_time");
	portal_u_size = glGetUniformLocation(portal_program, "u_size");
	
	puddle_program = create_program_from_disk("/home/pi/portal/projector/puddle.vert","/home/pi/portal/projector/puddle.frag");
	link_program(puddle_program);
	puddle_in_TexCoord = glGetAttribLocation(puddle_program, "in_TexCoord");
	puddle_in_Position = glGetAttribLocation(puddle_program, "in_Position");
	puddle_u_time = glGetUniformLocation(puddle_program, "u_time");
	puddle_u_Alpha = glGetUniformLocation(puddle_program, "u_Alpha");
	puddle_u_blue = glGetUniformLocation(puddle_program, "u_blue");
	puddle_u_size = glGetUniformLocation(puddle_program, "u_size");
	
	//upload data for video, puddle, and portal
	glGenBuffers(1, &vbo);
	
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices) + sizeof(vTexCoords), 0, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vVertices), &vVertices[0]);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(vVertices), sizeof(vTexCoords), &vTexCoords[0]);

	//particle program setup
	particles_program = create_program_from_disk("/home/pi/portal/projector/particles.vert","/home/pi/portal/projector/particles.frag");
	link_program(particles_program);
	particles_vertex = glGetAttribLocation(particles_program, "vertex");	
	particles_color = glGetAttribLocation(particles_program, "in_color");
	particle_sprite = glGetUniformLocation(particles_program, "u_Texture");
	
	//data for particles
	for(int i=0; i < NUM_PARTICLES; i++){
		particles[i].angle = ((float)(rand() % 36000))/100;
		particles[i].r = .7 + .1* (float)(rand() % 1000) /1000;
		particles[i].angle_velocity = 1.0 + .2* (float)(rand() % 1000) /1000;
		particles[i].r_velocity = -.001 + 0.002* (float)(rand() % 1000) /1000;
		
	}
	//load the colors, orange and blue are the same except for swapping r and b
	for(int i =0; i < NUM_PARTICLES; i++){
		particles_colors_orange[i*4+2] = particles_colors_blue[i*4+0] = 0.25;
		particles_colors_orange[i*4+1] = particles_colors_blue[i*4+1] = 0.35;
		particles_colors_orange[i*4+0] = particles_colors_blue[i*4+2] = 1.0;
		particles_colors_orange[i*4+3] = particles_colors_blue[i*4+3] = 1.0;
	}
	for(int i = NUM_PARTICLES; i < NUM_PARTICLES * 2; i++){
		particles_colors_orange[i*4+2] = particles_colors_blue[i*4+0] = 0.4;
		particles_colors_orange[i*4+1] = particles_colors_blue[i*4+1] = 0.8;
		particles_colors_orange[i*4+0] = particles_colors_blue[i*4+2] = 1.0;
		particles_colors_orange[i*4+3] = particles_colors_blue[i*4+3] = 1.0;
	}
	for(int i = NUM_PARTICLES * 2; i < NUM_PARTICLES * 3; i++){
		particles_colors_orange[i*4+2] = particles_colors_blue[i*4+0] = 0.5;
		particles_colors_orange[i*4+1] = particles_colors_blue[i*4+1] = 0.5;
		particles_colors_orange[i*4+0] = particles_colors_blue[i*4+2] = 0.5;
		particles_colors_orange[i*4+3] = particles_colors_blue[i*4+3] = 1.0;
	}

	glGenBuffers(1, &particles_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, particles_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(particles_vertices)+sizeof(particles_colors_orange)+sizeof(particles_colors_blue), 0, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0															, sizeof(particles_vertices)	 , &particles_vertices[0]);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(particles_vertices)									, sizeof(particles_colors_orange), &particles_colors_orange[0]);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(particles_vertices)+sizeof(particles_colors_orange)	, sizeof(particles_colors_blue)	 , &particles_colors_blue[0]);
	
	particle_sprite = png_load("/home/pi/portal/projector/circle64.png", NULL, NULL);
	
	//fire up gstreamer
	gstcontext_init(egl.display, egl.context, &gstcontext_texture_id, &gstcontext_texture_fresh, &this_gun->video_done);
	projector_logic_init(&this_gun->video_done);
	
	egl.draw = projector_scene_draw;

	this_gun->projector_loaded = true;

	return &egl;
}
