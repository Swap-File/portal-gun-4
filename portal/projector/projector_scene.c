#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "../common/memory.h"
#include "../common/common.h"
#include "../common/esUtil.h"
#include "../common/gstcontext.h"

#include "projector_logic.h"
#include "projector_scene.h"
#include "png.h"

struct gun_struct *this_gun; 
static volatile GLint gstcontext_texture_id; //in gstcontext
static volatile bool gstcontext_texture_fresh; //in gstcontext

//textures
static GLuint orange_1,blue_n,orange_n,orange_0,blue_0,blue_1,orange_portal,blue_portal,circle64;
static struct egl egl;
static GLfloat aspect;
static GLuint program;

/* uniform handles: */
static GLint modelviewmatrix, modelviewprojectionmatrix, normalmatrix;
static GLint texture;
static GLuint vbo;
static GLuint positionsoffset, texcoordsoffset, normalsoffset;

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
		-1.0f, -1.0f, 0.0f,
		+1.0f, -1.0f, 0.0f,
		-1.0f, +1.0f, 0.0f,
		+1.0f, +1.0f, 0.0f,
		// front
		-1.0f, -1.0f, 0.0f,
		+1.0f, -1.0f, 0.0f,
		-1.0f, +1.0f, 0.0f,
		+1.0f, +1.0f, 0.0f,
		// front
		-1.0f, -1.0f, 0.0f,
		+1.0f, -1.0f, 0.0f,
		-1.0f, +1.0f, 0.0f,
		+1.0f, +1.0f, 0.0f,
		// front
		-1.0f, -1.0f, 0.0f,
		+1.0f, -1.0f, 0.0f,
		-1.0f, +1.0f, 0.0f,
		+1.0f, +1.0f, 0.0f,
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
		1.0f, 1.0f,
		0.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,
		//left
		1.0f, 1.0f,
		0.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,
		//top
		1.0f, 1.0f,
		0.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,
		//bottom
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
		// back
		+0.0f, +0.0f, +1.0f, // forward
		+0.0f, +0.0f, +1.0f, // forward
		+0.0f, +0.0f, +1.0f, // forward
		+0.0f, +0.0f, +1.0f, // forward
		// right
		+0.0f, +0.0f, +1.0f, // forward
		+0.0f, +0.0f, +1.0f, // forward
		+0.0f, +0.0f, +1.0f, // forward
		+0.0f, +0.0f, +1.0f, // forward
		// left
		+0.0f, +0.0f, +1.0f, // forward
		+0.0f, +0.0f, +1.0f, // forward
		+0.0f, +0.0f, +1.0f, // forward
		+0.0f, +0.0f, +1.0f, // forward
		// top
		+0.0f, +0.0f, +1.0f, // forward
		+0.0f, +0.0f, +1.0f, // forward
		+0.0f, +0.0f, +1.0f, // forward
		+0.0f, +0.0f, +1.0f, // forward
		// bottom
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
		usleep(500);
		if (millis() - start_time > 20){
			//printf("Frameskip\n");
			break;
		}
	}

	logic_update(this_gun->gst_state,this_gun->portal_state);
	
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
	
	ESMatrix modelview;
	esMatrixLoadIdentity(&modelview);	
	ESMatrix modelviewprojection;
	esMatrixLoadIdentity(&modelviewprojection);
    esOrtho(&modelviewprojection, -1366/2, 1366/2,-768/2, 768/2, -1,1);
 
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
	//glUniform1i(texture, 0); /* '0' refers to texture unit 0. */
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	
	
	esMatrixLoadIdentity(&modelview);
	esTranslate(&modelview, 0.0f, 0.0f, -8.0f);
	const float scale = 4.8f;
	esScale(&modelview,scale * 1366/768,scale,1.0f);
	esRotate(&modelview, rotation , 0.0f, 0.0f, 1.0f);
	//esScale(&modelview,100,100,1);
	ESMatrix projection;
	esMatrixLoadIdentity(&projection);
	esFrustum(&projection, -2.8f, +2.8f, -2.8f * aspect, +2.8f * aspect, 6.0f, 10.0f);

	esMatrixLoadIdentity(&modelviewprojection);
	esMatrixMultiply(&modelviewprojection, &modelview, &projection);

		glUniformMatrix4fv(modelviewmatrix, 1, GL_FALSE, &modelview.m[0][0]);
	glUniformMatrix4fv(modelviewprojectionmatrix, 1, GL_FALSE, &modelviewprojection.m[0][0]);
	
	glBindTexture(GL_TEXTURE_2D, blue_portal);
	glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
	
	//glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);
	//glDrawArrays(GL_TRIANGLE_STRIP, 12, 4);
	//glDrawArrays(GL_TRIANGLE_STRIP, 16, 4);
	//glDrawArrays(GL_TRIANGLE_STRIP, 20, 4);
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
	

	gstcontext_texture_fresh = false;

}


const struct egl * scene_init(const struct gbm *gbm, int samples)
{
	
	shared_init(&this_gun,false);
	this_gun->gst_state = 1;
	
	fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);
	
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
	gstcontext_init(egl.display, egl.context, &gstcontext_texture_id, &gstcontext_texture_fresh, &this_gun->video_done);
	
	logic_init(&this_gun->video_done);

	//load textures
	
	orange_n = png_load("/home/pi/assets/orange_n.png", NULL, NULL);
	//override default of clamp
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	
	blue_n = png_load("/home/pi/assets/blue_n.png", NULL, NULL);
	//override default of clamp
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	
	orange_0 = png_load("/home/pi/assets/orange_0.png", NULL, NULL);
	orange_1 = png_load("/home/pi/assets/orange_1.png", NULL, NULL);

	blue_0 = png_load("/home/pi/assets/blue_0.png", NULL, NULL);
	blue_1 = png_load("/home/pi/assets/blue_1.png", NULL, NULL);
	
	orange_portal = png_load("/home/pi/assets/orange_portal.png", NULL, NULL);
	blue_portal   = png_load("/home/pi/assets/blue_portal.png",   NULL, NULL);
	
	circle64 = png_load("/home/pi/assets/circle64.png", NULL, NULL);
	
	if (orange_n == 0 || blue_n == 0 || orange_portal == 0 || blue_portal == 0 || orange_0 == 0 || orange_1 == 0 || blue_0 == 0 || blue_1 == 0 || circle64 == 0)
	{
		printf("Loading textures failed.\n");
		exit(1);
	}
	
	egl.draw = scene_draw;
	
	this_gun->projector_loaded = true;
		
	return &egl;
}
