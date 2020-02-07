#version 310 es
uniform mat4 modelviewprojectionMatrix;
in vec4 in_position;
in vec2 in_TexCoord;
out vec2 vTexCoord;
void main()
{
	gl_Position = modelviewprojectionMatrix * in_position;
	vTexCoord = in_TexCoord;
}