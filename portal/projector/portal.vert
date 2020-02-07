#version 310 es
uniform mat4 u_mvpMatrix;
in vec4 in_Position;
in vec2 in_TexCoord;
out vec2 vUv;
void main()
{
	vUv = in_TexCoord;
	gl_Position =  in_Position;
}