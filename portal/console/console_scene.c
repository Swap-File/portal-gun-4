#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "../common/memory.h"
#include "../common/opengl.h"
#include "../common/esUtil.h"
#include "../common/gstcontext.h"
#include "../common/effects.h"

#include "console_logic.h"
#include "console_scene.h"
#include "font.h"

struct gun_struct *this_gun; 
static volatile GLint gstcontext_texture_id; //passed to & set in gstcontext
static volatile bool gstcontext_texture_fresh; //passed to & set in gstcontext

static struct atlas a64; //normal font
static struct atlas a64b; //bold font

static struct egl egl;

/* uniform handles: */
static GLuint basic_program,basic_u_mvpMatrix,basic_in_Position,basic_in_TexCoord,basic_u_Texture;

static GLuint vbo;
static GLuint positionsoffset, texcoordsoffset;

#define VIDEO_WIDTH 640
#define VIDEO_HEIGHT 480
static const GLfloat vVertices[] = {
	// front
	-VIDEO_WIDTH/2, -VIDEO_HEIGHT/2, 0.0f,
	+VIDEO_WIDTH/2, -VIDEO_HEIGHT/2, 0.0f,
	-VIDEO_WIDTH/2, +VIDEO_HEIGHT/2, 0.0f,
	+VIDEO_WIDTH/2, +VIDEO_HEIGHT/2, 0.0f,
};

GLfloat vTexCoords[] = {
	//front
	1.0f, 1.0f,
	0.0f, 1.0f,
	1.0f, 0.0f,
	0.0f, 0.0f,
};

//each char is 0.145833 wide, screen is 2 wide, .05 on either side
void center_text(struct atlas * a, char * text, float line)
{
	const float sx = 2.0 / 480;
	const float sy = 2.0 / 640;
	float space = 2.0 - font_length(text,a,sx,sy);
	font_render(text,a, -1 +( space / 2 ), 1 - (630 - 64 * line) * sy, sx, sy);
}

static inline float slide(float target, float color_delta, float now){
	if (target < now)		return MAX2(now - color_delta,target);
	else if (target > now)	return MIN2(now + color_delta,target);
	return now;
}

static void slide_to(float r_target, float g_target, float b_target)
{
	const int speed = 1000;
	static float r_now,g_now,b_now;
	static uint32_t last_frame_time = 0;
	
	uint32_t this_frame_time = millis();
	uint32_t time_delta = this_frame_time - last_frame_time;
	float color_delta = (float)time_delta / speed; //must cast to float!
	
	r_now = slide(r_target,color_delta,r_now);
	g_now = slide(g_target,color_delta,g_now);
	b_now = slide(b_target,color_delta,b_now);
	
	last_frame_time = this_frame_time;
	
	glClearColor(r_now,g_now,b_now,0.0); 
}

static void draw_emitter(int line){
	char temp[20];	

	if ( this_gun->laser_countdown != 0 ) {
		static uint32_t last_update = 0;
		static uint32_t animation = 0;
		
		int milliseconds = (this_gun->laser_countdown % 1000);
		int seconds      = (this_gun->laser_countdown / 1000) % 60;
		
		if (millis() - last_update > 50){
			animation++;
			if (animation > 3) animation = 0;
			last_update = millis();
		}
		
		if (animation == 0) sprintf(temp,">  -%02d:%03d  <", seconds,milliseconds);
		if (animation == 1) sprintf(temp," > -%02d:%03d < ", seconds,milliseconds);
		if (animation == 2) sprintf(temp,"  >-%02d:%03d<  ", seconds,milliseconds);
		if (animation == 3) sprintf(temp,"   -%02d:%03d   ", seconds,milliseconds);
	
	}else if ( abs(this_gun->state_solo) > 3 || this_gun->state_duo > 3 ) {
		sprintf(temp,"Emitting");
	}else if (this_gun->state_duo < -1) {
		sprintf(temp,"Capturing");
	}else{
		if (this_gun->laser_on)
		sprintf(temp,"Warm");
		else
		sprintf(temp,"Idle");
	}
	center_text(&a64,temp, line);
}

static void draw_scene(unsigned i,char *debug_msg)
{
	if (debug_msg[0] != '\0'){
		int temp[2];
		int result = sscanf(debug_msg,"%d %d", &temp[0],&temp[1]);
		if (result == 2){
			printf("\nDebug Message Parsed: Setting state_duo: %d and ui_mode: %d\n",temp[0],temp[1]);
			this_gun->state_duo = temp[0];
			this_gun->gst_state = temp[1];
		}
	}
	
	if		(this_gun->state_solo < 0 || this_gun->state_duo < 0)	slide_to(0.0,0.5,0.5); 
	else if (this_gun->state_solo > 0 || this_gun->state_duo > 0)	slide_to(0.8,0.4,0.0); 
	else 															slide_to(0.0,0.0,0.0); 
	
	console_logic(this_gun->gst_state);
	uint32_t render_start_time = micros();
	
	glClear(GL_COLOR_BUFFER_BIT);
	// Enable blending, necessary for our alpha texture
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	if (this_gun->gst_state == GST_RPICAMSRC){
		ESMatrix modelviewprojection;
		esMatrixLoadIdentity(&modelviewprojection);
		esOrtho(&modelviewprojection, -640/2, 640/2,-480/2, 480/2, -1,1);
		glUseProgram(basic_program);
		//glActiveTexture(GL_TEXTURE0);
		glBindTexture( GL_TEXTURE_2D, gstcontext_texture_id);
		glUniform1i(basic_u_Texture, 0); /* '0' refers to texture unit 0. */
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glEnableVertexAttribArray(basic_in_Position);
		glVertexAttribPointer(basic_in_Position, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)positionsoffset);
		glEnableVertexAttribArray(basic_in_TexCoord);
		glVertexAttribPointer(basic_in_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)texcoordsoffset);		
		glUniformMatrix4fv(basic_u_mvpMatrix, 1, GL_FALSE, &modelviewprojection.m[0][0]);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glDisableVertexAttribArray(basic_in_Position);
		glDisableVertexAttribArray(basic_in_TexCoord);
		
	} else {

		GLfloat white[4] = { 1, 1, 1, 1 };
		text_color(white); 
		
		char temp[20];	
		if (this_gun->gordon)	center_text(&a64b,"Gordon", 9);
		else 					center_text(&a64b,"Chell", 9);
		
		if (this_gun->ui_mode == UI_SIMPLE){
			
			if (this_gun->mode == MODE_SOLO){
				if (this_gun->state_solo == 0 && this_gun->state_duo == 0) {
					center_text(&a64,"1 1P Orange", 8);
					center_text(&a64,"2 1P Blue  ", 7);
					center_text(&a64,"3 -> Duo   ", 6);
					center_text(&a64,"4 Reset    ", 5);
				}
				if (abs(this_gun->state_solo) > 0 && abs(this_gun->state_solo) < 3 && this_gun->state_duo == 0) {
					center_text(&a64,"1 Next Solo", 8);
					center_text(&a64,"2          ", 7);
					center_text(&a64,"3 -> UI    ", 6);
					center_text(&a64,"4 Reset    ", 5);
				}
				if (abs(this_gun->state_solo) == 3 && this_gun->state_duo == 0) {
					center_text(&a64,"1 Waiting  ", 8);
					center_text(&a64,"2          ", 7);
					center_text(&a64,"3 -> UI    ", 6);
					center_text(&a64,"4 Reset    ", 5);
				}
				if (abs(this_gun->state_solo) == 4 && this_gun->state_duo == 0) {
					center_text(&a64,"1 Next FX  ", 8);
					center_text(&a64,"2 ColorSwap", 7);
					center_text(&a64,"3 -> UI    ", 6);
					center_text(&a64,"4 Reset    ", 5);
				}
				if (abs(this_gun->state_solo) == 5 && this_gun->state_duo == 0) {
					center_text(&a64,"1 Open Solo", 8);
					center_text(&a64,"2 Color Sw ", 7);
					center_text(&a64,"3 -> UI    ", 6);
					center_text(&a64,"4 Reset    ", 5);
				}
			} else {
				if (this_gun->state_duo == 0 && this_gun->state_solo == 0) {
					center_text(&a64,"1 Projector", 8);
					center_text(&a64,"2 Camera   ", 7);
					center_text(&a64,"3 -> Solo  ", 6);
					center_text(&a64,"4 Reset    ", 5);
				}
				if ((this_gun->state_duo == -1 || this_gun->state_duo == -2) && this_gun->state_solo == 0) {
					center_text(&a64,"1 Camera   ", 8);
					center_text(&a64,"2          ", 7);
					center_text(&a64,"3 -> UI    ", 6);
					center_text(&a64,"4 Reset    ", 5);
				}
				if (this_gun->state_duo == -3 && this_gun->state_solo == 0 ) {
					center_text(&a64,"1 Waiting  ", 8);
					center_text(&a64,"2          ", 7);
					center_text(&a64,"3 -> UI    ", 6);
					center_text(&a64,"4 Reset    ", 5);
				}
				if (this_gun->state_solo == -4 && this_gun->state_solo == 0 ) {
					center_text(&a64,"1          ", 8);
					center_text(&a64,"2 Projector", 7);
					center_text(&a64,"3 -> UI    ", 6);
					center_text(&a64,"4 Reset    ", 5);
				}					
				if ((this_gun->state_duo == 1 || this_gun->state_duo == 2) && this_gun->state_solo == 0 ) {
					center_text(&a64,"1 Projector", 8);
					center_text(&a64,"2          ", 7);
					center_text(&a64,"3 -> UI    ", 6);
					center_text(&a64,"4 Reset    ", 5);
				}	
				if ((this_gun->state_duo == 3 || this_gun->state_duo == 4) && this_gun->state_solo == 0 ) {
					center_text(&a64,"1 Waiting  ", 8);
					center_text(&a64,"2          ", 7);
					center_text(&a64,"3 -> UI    ", 6);
					center_text(&a64,"4 Reset    ", 5);
				}
				if ((this_gun->state_duo == 5) && this_gun->state_solo == 0 ) {
					center_text(&a64,"1          ", 8);
					center_text(&a64,"2 Next FX  ", 7);
					center_text(&a64,"3 -> UI    ", 6);
					center_text(&a64,"4 Reset    ", 5);
				}	
				if ((this_gun->state_duo == 6) && this_gun->state_solo == 0 ) {
					center_text(&a64,"1 Open     ", 8);
					center_text(&a64,"2          ", 7);
					center_text(&a64,"3 -> UI    ", 6);
					center_text(&a64,"4 Reset    ", 5);
				}	
	
			}
			
			draw_emitter(3);
			
		} else {
			sprintf(temp,"%.0f/%.0f\260F",this_gun->temperature_pretty ,this_gun->coretemp);	
			center_text(&a64,temp, 8);
			
			if (this_gun->connected) sprintf(temp,"Synced");	
			else  sprintf(temp,"Sync Err");	
			center_text(&a64,temp, 7);

			draw_emitter(6);
			
			int kbits = this_gun->kbytes_wlan;
			if (this_gun->kbytes_bnep > 0)  kbits += this_gun->kbytes_bnep;
			sprintf(temp,"%dKb/s",kbits);	
			center_text(&a64,temp, 5);
			
			sprintf(temp,"%ddB %dMB/s",this_gun->dbm ,this_gun->tx_bitrate);	 
			center_text(&a64,temp, 4);
			
			if (this_gun->latency > 99) sprintf(temp,"%.1fV %.0fms",this_gun->battery_level_pretty,this_gun->latency );	
			else if (this_gun->latency > 9) sprintf(temp,"%.1fV %.1fms",this_gun->battery_level_pretty,this_gun->latency );
			else  sprintf(temp,"%.1fV %.2fms",this_gun->battery_level_pretty,this_gun->latency );
			center_text(&a64,temp, 3);
			

			
		}
		
		if (this_gun->mode == MODE_DUO) sprintf(temp,"%s",effectnames[this_gun->effect_duo]);
		else							sprintf(temp,"%s",effectnames[this_gun->effect_solo]);		
		center_text(&a64,temp, 2);
			
		uint32_t current_time;	
		if (this_gun->mode == MODE_DUO)  {
			sprintf(temp,"Duo Mode");	
			if (this_gun->connected) current_time = (millis() + this_gun->other_gun_clock)/2;
			else current_time = millis();  
		}
		else   {  
			sprintf(temp,"Solo Mode");	
			current_time = millis();
		}
		center_text(&a64,temp, 1);
		
		int milliseconds = (current_time % 1000);
		int seconds      = (current_time / 1000) % 60;
		int minutes      = (current_time / (1000*60)) % 60;
		int hours        = (current_time / (1000*60*60)) % 24;
		sprintf(temp,"%.2d:%.2d:%.2d.%.3d",hours,minutes,seconds,milliseconds);	
		center_text(&a64,temp, 0);
	}
	
	/* FPS counter */
	glFinish();
	fps_counter("Console:   ",render_start_time,false);
}


const struct egl * init_scene(const struct gbm *gbm, int samples)
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
	basic_u_mvpMatrix = glGetUniformLocation(basic_program, "u_mvpMatrix");
	basic_in_Position = glGetAttribLocation(basic_program, "in_Position");
	basic_in_TexCoord = glGetAttribLocation(basic_program, "in_TexCoord");
	
	positionsoffset = 0;
	texcoordsoffset = sizeof(vVertices);

	//upload data
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices) + sizeof(vTexCoords), 0, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, positionsoffset, sizeof(vVertices), &vVertices[0]);
	glBufferSubData(GL_ARRAY_BUFFER, texcoordsoffset, sizeof(vTexCoords), &vTexCoords[0]);

	//fire up gstreamer 
	gstcontext_init(egl.display, egl.context, &gstcontext_texture_id, &gstcontext_texture_fresh, NULL);
	console_logic_init(this_gun->gordon);
	
	font_init("/home/pi/assets/consola.ttf");
	font_atlas_init(64,&a64);
	font_init("/home/pi/assets/consolab.ttf");
	font_atlas_init(64,&a64b); 
	
	egl.draw = draw_scene;
	
	this_gun->console_loaded = true;
	
	return &egl;
}
