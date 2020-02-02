#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "../common/memory.h"
#include "../common/common.h"
#include "../common/esUtil.h"
#include "../common/gstcontext.h"
#include "../common/effects.h"

#include "console_logic.h"
#include "console_scene.h"
#include "font.h"

struct gun_struct *this_gun; 
static volatile GLint gstcontext_texture_id; //in gstcontext
static volatile bool gstcontext_texture_fresh; //in gstcontext

static struct atlas a64;
static struct atlas a64b;

static struct egl egl;

static GLfloat aspect;
static GLuint program;

/* uniform handles: */
static GLint modelviewmatrix, modelviewprojectionmatrix, normalmatrix;
static GLint texture;
static GLuint vbo;
static GLuint positionsoffset, texcoordsoffset, normalsoffset;

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

static const GLfloat vNormals[] = {
	// front
	+0.0f, +0.0f, +1.0f, // forward
	+0.0f, +0.0f, +1.0f, // forward
	+0.0f, +0.0f, +1.0f, // forward
	+0.0f, +0.0f, +1.0f, // forward
};

static const char *vertex_shader_source =
"uniform mat4 modelviewMatrix;                                 \n"
"uniform mat4 modelviewprojectionMatrix;                       \n"
"uniform mat3 normalMatrix;                                    \n"
"                                                              \n"
"attribute vec4 in_position;                                   \n"
"attribute vec3 in_normal;                                     \n"
"attribute vec2 in_TexCoord;                                   \n"
"                                                              \n"
"vec4 lightSource = vec4(2.0, 2.0, 20.0, 0.0);                 \n"
"                                                              \n"
"varying vec4 vVaryingColor;                                   \n"
"varying vec2 vTexCoord;                                       \n"
"                                                              \n"
"void main()                                                   \n"
"{                                                             \n"
"    gl_Position = modelviewprojectionMatrix * in_position;    \n"
"    vec3 vEyeNormal = normalMatrix * in_normal;               \n"
"    vec4 vPosition4 = modelviewMatrix * in_position;          \n"
"    vec3 vPosition3 = vPosition4.xyz / vPosition4.w;          \n"
"    vec3 vLightDir = normalize(lightSource.xyz - vPosition3); \n"
"    float diff = max(0.0, dot(vEyeNormal, vLightDir));        \n"
"    vVaryingColor = vec4(diff * vec3(1.0, 1.0, 1.0), 1.0);    \n"
"    vTexCoord = in_TexCoord;                                  \n"
"}                                                             \n";

static const char *fragment_shader_source =
"precision mediump float;                            \n"
"varying vec2 vTexCoord;                             \n"
"uniform sampler2D s_texture;                        \n"
"void main()                                         \n"
"{                                                   \n"
"  gl_FragColor = texture2D( s_texture, vTexCoord ); \n"
"}                                                   \n";

//each char is 0.145833 wide, screen is 2 wide, .05 on either side
void center_text(struct atlas * a, char * text, float line)
{
	const float sx = 2.0 / 480;
	const float sy = 2.0 / 640;
	float space = 2.0 - font_length(text,a,sx,sy);
	font_render(text,a, -1 +( space / 2 ), 1 - (630 - 64 * line) * sy, sx, sy);
}

static void slide_to(float r_target, float g_target, float b_target)
{
	const int speed = 1000;
	static float r_now,g_now,b_now;
	static uint32_t last_frame_time = 0;
	
	uint32_t this_frame_time = millis();
	uint32_t time_delta = this_frame_time - last_frame_time;
	float color_delta = (float)time_delta / speed; //must cast to float!
	
	if (r_target < r_now)		r_now = MAX2(r_now - color_delta,r_target);
	else if (r_target > r_now)	r_now = MIN2(r_now + color_delta,r_target);
	
	if (g_target < g_now)		g_now = MAX2(g_now - color_delta,g_target);
	else if (g_target > g_now)	g_now = MIN2(g_now + color_delta,g_target);
	
	if (b_target < b_now)		b_now = MAX2(b_now - color_delta,b_target);
	else if (b_target > b_now)	b_now = MIN2(b_now + color_delta,b_target);
	
	last_frame_time = this_frame_time;
	
	glClearColor(r_now,g_now,b_now,0.0); 
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
	
	glClear(GL_COLOR_BUFFER_BIT);
	// Enable blending, necessary for our alpha texture
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	if (this_gun->gst_state == GST_RPICAMSRC){
		ESMatrix modelview;
		esMatrixLoadIdentity(&modelview);
		ESMatrix modelviewprojection;
		esMatrixLoadIdentity(&modelviewprojection);
		esOrtho(&modelviewprojection, -640/2, 640/2,-480/2, 480/2, -1,1);
		
		float normal[9];
		normal[0] = modelview.m[0][0];
		normal[1] = modelview.m[0][1];
		normal[2] = modelview.m[0][2];
		normal[3] = modelview.m[1][0];
		normal[4] = modelview.m[1][1];
		normal[5] = modelview.m[1][2];
		normal[6] = modelview.m[2][0];
		normal[7] = modelview.m[2][1];
		normal[8] = modelview.m[2][2];
		
		glUseProgram(program);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture( GL_TEXTURE_2D, gstcontext_texture_id);	
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)positionsoffset);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)normalsoffset);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)texcoordsoffset);
		glEnableVertexAttribArray(2);
		glUniformMatrix4fv(modelviewmatrix, 1, GL_FALSE, &modelview.m[0][0]);
		glUniformMatrix4fv(modelviewprojectionmatrix, 1, GL_FALSE, &modelviewprojection.m[0][0]);
		glUniformMatrix3fv(normalmatrix, 1, GL_FALSE, normal);
		//glUniform1i(gl.texture, 0); /* '0' refers to texture unit 0. */
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);
		
	} else {

		GLfloat white[4] = { 1, 1, 1, 1 };
		text_color(white); 
		
		char temp[20];	
		if (this_gun->gordon)	center_text(&a64b,"Gordon", 9);
		else 					center_text(&a64b,"Chell", 9);
		
		if (this_gun->ui_mode == UI_SIMPLE){
			
			center_text(&a64,"TRAINER", 8);
			center_text(&a64,"GUI", 7);
			center_text(&a64,"PLACE", 6);
			center_text(&a64,"HOLDER", 5);
			
		}else{
			sprintf(temp,"%.0f/%.0f\260F",this_gun->temperature_pretty ,this_gun->coretemp);	
			center_text(&a64,temp, 8);
			
			if (this_gun->connected) sprintf(temp,"Synced");	
			else  sprintf(temp,"Sync Err");	
			center_text(&a64,temp, 7);

			if ( abs(this_gun->state_solo) > 2 || this_gun->state_duo > 2 ) {
				sprintf(temp,"Emitting");
			}
			else if ( abs(this_gun->state_solo) == 2 || this_gun->state_duo == 2 ) {
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
				
			}else if (this_gun->state_duo < -1) {
				sprintf(temp,"Capturing");
			}else{
				sprintf(temp,"Idle");
			}
			center_text(&a64,temp, 6);
			
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
			
			if (this_gun->mode == MODE_DUO) sprintf(temp,"%s",effectnames[this_gun->effect_duo]);
			else					sprintf(temp,"%s",effectnames[this_gun->effect_solo]);		
			center_text(&a64,temp, 2);
			
		}
		
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
	
	gstcontext_texture_fresh = false;
}


const struct egl * init_scene(const struct gbm *gbm, int samples)
{
	shared_init(&this_gun,false);
	this_gun->gst_state = 0;
	
	int ret = init_egl(&egl, gbm, samples);
	if (ret)
	return NULL;

	if (egl_check(&egl, eglCreateImageKHR) ||
			egl_check(&egl, glEGLImageTargetTexture2DOES) ||
			egl_check(&egl, eglDestroyImageKHR))
	return NULL;
	
	aspect = (GLfloat)(gbm->height) / (GLfloat)(gbm->width);

	gbm = gbm;

	program = create_program(vertex_shader_source, fragment_shader_source);

	glBindAttribLocation(program, 0, "in_position");
	glBindAttribLocation(program, 1, "in_normal");
	glBindAttribLocation(program, 2, "in_color");

	link_program(program);

	modelviewmatrix = glGetUniformLocation(program, "modelviewMatrix");
	modelviewprojectionmatrix = glGetUniformLocation(program, "modelviewprojectionMatrix");
	normalmatrix = glGetUniformLocation(program, "normalMatrix");
	texture   = glGetUniformLocation(program, "uTex");
	
	//glViewport(0, 0, gbm->width, gbm->height);
	//glEnable(GL_CULL_FACE);

	positionsoffset = 0;
	texcoordsoffset = sizeof(vVertices);
	normalsoffset = sizeof(vVertices) + sizeof(vTexCoords);

	//upload data
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices) + sizeof(vTexCoords) + sizeof(vNormals), 0, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, positionsoffset, sizeof(vVertices), &vVertices[0]);
	glBufferSubData(GL_ARRAY_BUFFER, texcoordsoffset, sizeof(vTexCoords), &vTexCoords[0]);
	glBufferSubData(GL_ARRAY_BUFFER, normalsoffset, sizeof(vNormals), &vNormals[0]);
	
	//fire up gstreamer 
	gstcontext_init(egl.display, egl.context, &gstcontext_texture_id, &gstcontext_texture_fresh, NULL);
	console_logic_init(this_gun->gordon);
	
	font_init("/home/pi/assets/consola.ttf");
	font_atlas_init(64,&a64);
	font_init("/home/pi/assets/consolab.ttf");
	font_atlas_init(64,&a64b); 
	
	egl.draw = draw_scene;
	return &egl;
}
