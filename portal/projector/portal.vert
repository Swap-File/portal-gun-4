#version 310 es
uniform mat4 modelviewprojectionMatrix;
in vec4 in_position;
in vec2 in_TexCoord;
out vec2 vUv;
void main()
{
	vUv = in_TexCoord;
	gl_Position = in_position;
}