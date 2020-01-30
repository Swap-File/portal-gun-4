#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "../common/memory.h"
#include "../common/common.h"
#include "../common/esUtil.h"
#include "console_logic.h"
#include "console_scene.h"
#include "../common/gstcontext.h"
#include "font.h"

struct gun_struct *this_gun; 
static volatile GLint gstcontext_texture_id; //in gstcontext
static volatile bool gstcontext_texture_fresh; //in gstcontext

static struct atlas a128;
static struct atlas a128b;

static struct egl egl;

static GLfloat aspect;
static GLuint program;
	
/* uniform handles: */
static GLint modelviewmatrix, modelviewprojectionmatrix, normalmatrix;
static GLint texture;
static GLuint vbo;
static GLuint positionsoffset, texcoordsoffset, normalsoffset;

static const GLfloat vVertices[] = {
		// front
		-1.0f, -1.0f, +1.0f,
		+1.0f, -1.0f, +1.0f,
		-1.0f, +1.0f, +1.0f,
		+1.0f, +1.0f, +1.0f,
		// back
		+1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		+1.0f, +1.0f, -1.0f,
		-1.0f, +1.0f, -1.0f,
		// right
		+1.0f, -1.0f, +1.0f,
		+1.0f, -1.0f, -1.0f,
		+1.0f, +1.0f, +1.0f,
		+1.0f, +1.0f, -1.0f,
		// left
		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f, +1.0f,
		-1.0f, +1.0f, -1.0f,
		-1.0f, +1.0f, +1.0f,
		// top
		-1.0f, +1.0f, +1.0f,
		+1.0f, +1.0f, +1.0f,
		-1.0f, +1.0f, -1.0f,
		+1.0f, +1.0f, -1.0f,
		// bottom
		-1.0f, -1.0f, -1.0f,
		+1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f, +1.0f,
		+1.0f, -1.0f, +1.0f,
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
		1.0f, 0.0f,
		0.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
};

static const GLfloat vNormals[] = {
		// front
		+0.0f, +0.0f, +1.0f, // forward
		+0.0f, +0.0f, +1.0f, // forward
		+0.0f, +0.0f, +1.0f, // forward
		+0.0f, +0.0f, +1.0f, // forward
		// back
		+0.0f, +0.0f, -1.0f, // backward
		+0.0f, +0.0f, -1.0f, // backward
		+0.0f, +0.0f, -1.0f, // backward
		+0.0f, +0.0f, -1.0f, // backward
		// right
		+1.0f, +0.0f, +0.0f, // right
		+1.0f, +0.0f, +0.0f, // right
		+1.0f, +0.0f, +0.0f, // right
		+1.0f, +0.0f, +0.0f, // right
		// left
		-1.0f, +0.0f, +0.0f, // left
		-1.0f, +0.0f, +0.0f, // left
		-1.0f, +0.0f, +0.0f, // left
		-1.0f, +0.0f, +0.0f, // left
		// top
		+0.0f, +1.0f, +0.0f, // up
		+0.0f, +1.0f, +0.0f, // up
		+0.0f, +1.0f, +0.0f, // up
		+0.0f, +1.0f, +0.0f, // up
		// bottom
		+0.0f, -1.0f, +0.0f, // down
		+0.0f, -1.0f, +0.0f, // down
		+0.0f, -1.0f, +0.0f, // down
		+0.0f, -1.0f, +0.0f  // down
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




static void draw_scene(unsigned i)
{

	glClearColor(0.3, 0.3, 0.3, 0.3);
	glClear(GL_COLOR_BUFFER_BIT);
	// Enable blending, necessary for our alpha texture
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	ESMatrix modelview;


	esMatrixLoadIdentity(&modelview);
	esTranslate(&modelview, 0.0f, 0.0f, -8.0f);
	esRotate(&modelview, 45.0f + (0.25f * i), 1.0f, 0.0f, 0.0f);
	esRotate(&modelview, 45.0f - (0.5f * i), 0.0f, 1.0f, 0.0f);
	esRotate(&modelview, 10.0f + (0.15f * i), 0.0f, 0.0f, 1.0f);

	ESMatrix projection;
	esMatrixLoadIdentity(&projection);
	esFrustum(&projection, -2.8f, +2.8f, -2.8f * aspect, +2.8f * aspect, 6.0f, 10.0f);

	ESMatrix modelviewprojection;
	esMatrixLoadIdentity(&modelviewprojection);
	esMatrixMultiply(&modelviewprojection, &modelview, &projection);

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
	//glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
	//glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);
	//glDrawArrays(GL_TRIANGLE_STRIP, 12, 4);
	//glDrawArrays(GL_TRIANGLE_STRIP, 16, 4);
	//glDrawArrays(GL_TRIANGLE_STRIP, 20, 4);
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
	
	float sx = 2.0 / 640;
	float sy = 2.0 / 480;

	//GLfloat black[4] = { 0, 0, 0, 1 };
	//GLfloat red[4] = { 1, 0, 0, 1 };
	GLfloat transparent_green[4] = { 0, 1, 0, 0.5 };
  
	text_color(transparent_green);  //also use program
	font_render("The Small Texture Scaled Fox Jumps Over The Lazy Dog", &a128, -1 + 8 * sx, 1 - 150 * sy, sx , sy);
	font_render("The Small Font Sized Fox Jumps Over The Lazy Dog", &a128b, -1 + 8 * sx, 1 - 400 * sy, sx , sy);

	gstcontext_texture_fresh = false;

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
	gstlogic_init();
	
	font_init("/home/pi/assets/consola.ttf");
	font_atlas_init(128,&a128);
	font_init("/home/pi/assets/consolab.ttf");
	font_atlas_init(128,&a128b); 
	
	egl.draw = draw_scene;
	return &egl;
}
