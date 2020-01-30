#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "../common/memory.h"
#include "../common/common.h"
#include "../common/esUtil.h"
#include "projector_logic.h"
#include "projector_scene.h"
#include "../common/gstcontext.h"
#include "png.h"

struct gun_struct *this_gun; 
static volatile GLint gstcontext_texture_id; //in gstcontext
static volatile bool gstcontext_texture_fresh; //in gstcontext

//textures
static GLuint orange_1,blue_n,orange_n,orange_0,blue_0,blue_1,texture_orange,texture_blue,circle64;


struct {
	struct egl egl;

	GLfloat aspect;
	const struct gbm *gbm;

	GLuint program;
	/* uniform handles: */
	GLint modelviewmatrix, modelviewprojectionmatrix, normalmatrix;
	GLint texture;
	GLuint vbo;
	GLuint positionsoffset, texcoordsoffset, normalsoffset;

} gl;

const struct egl *egl = &gl.egl;

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




static void scene_draw(unsigned i)
{

 
	while (gstcontext_texture_fresh == false){
 usleep(100);
}

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
	esFrustum(&projection, -2.8f, +2.8f, -2.8f * gl.aspect, +2.8f * gl.aspect, 6.0f, 10.0f);

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
		

	
	glUseProgram(gl.program);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture( GL_TEXTURE_2D, gstcontext_texture_id);
	glBindBuffer(GL_ARRAY_BUFFER, gl.vbo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)gl.positionsoffset);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)gl.normalsoffset);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)(intptr_t)gl.texcoordsoffset);
	glEnableVertexAttribArray(2);
	glUniformMatrix4fv(gl.modelviewmatrix, 1, GL_FALSE, &modelview.m[0][0]);
	glUniformMatrix4fv(gl.modelviewprojectionmatrix, 1, GL_FALSE, &modelviewprojection.m[0][0]);
	glUniformMatrix3fv(gl.normalmatrix, 1, GL_FALSE, normal);
	//glUniform1i(gl.texture, 0); /* '0' refers to texture unit 0. */
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindTexture(GL_TEXTURE_2D, orange_0);
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
	
	int ret = init_egl(&gl.egl, gbm, samples);
	if (ret)
		return NULL;

	if (egl_check(&gl.egl, eglCreateImageKHR) ||
	    egl_check(&gl.egl, glEGLImageTargetTexture2DOES) ||
	    egl_check(&gl.egl, eglDestroyImageKHR))
		return NULL;
	
	gl.aspect = (GLfloat)(gbm->height) / (GLfloat)(gbm->width);

	gl.gbm = gbm;

	gl.program = create_program(vertex_shader_source, fragment_shader_source);

	glBindAttribLocation(gl.program, 0, "in_position");
	glBindAttribLocation(gl.program, 1, "in_normal");
	glBindAttribLocation(gl.program, 2, "in_color");

	link_program(gl.program);

	gl.modelviewmatrix = glGetUniformLocation(gl.program, "modelviewMatrix");
	gl.modelviewprojectionmatrix = glGetUniformLocation(gl.program, "modelviewprojectionMatrix");
	gl.normalmatrix = glGetUniformLocation(gl.program, "normalMatrix");
	gl.texture   = glGetUniformLocation(gl.program, "uTex");
	
	//glViewport(0, 0, gbm->width, gbm->height);
	//glEnable(GL_CULL_FACE);

	gl.positionsoffset = 0;
	gl.texcoordsoffset = sizeof(vVertices);
	gl.normalsoffset = sizeof(vVertices) + sizeof(vTexCoords);

	//upload data
	glGenBuffers(1, &gl.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, gl.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices) + sizeof(vTexCoords) + sizeof(vNormals), 0, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, gl.positionsoffset, sizeof(vVertices), &vVertices[0]);
	glBufferSubData(GL_ARRAY_BUFFER, gl.texcoordsoffset, sizeof(vTexCoords), &vTexCoords[0]);
	glBufferSubData(GL_ARRAY_BUFFER, gl.normalsoffset, sizeof(vNormals), &vNormals[0]);
	
	
	//fire up gstreamer 
	gstcontext_init(gl.egl.display, gl.egl.context, &gstcontext_texture_id, &gstcontext_texture_fresh, NULL);
	logic_init();

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
	
	texture_orange = png_load("/home/pi/assets/orange_portal.png", NULL, NULL);
	texture_blue   = png_load("/home/pi/assets/blue_portal.png",   NULL, NULL);
	
	circle64 = png_load("/home/pi/assets/circle64.png", NULL, NULL);
	
	if (orange_n == 0 || blue_n == 0 || texture_orange == 0 || texture_blue == 0 || orange_0 == 0 || orange_1 == 0 || blue_0 == 0 || blue_1 == 0 || circle64 == 0)
	{
		printf("Loading textures failed.\n");
		exit(1);
	}
	
	gl.egl.draw = scene_draw;
	return &gl.egl;
}
