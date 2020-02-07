#version 310 es
uniform mat4 u_mvpMatrix;
in vec4 in_Position;
in vec2 in_TexCoord;
out vec2 vTexCoord;
void main()
{
	gl_Position = u_mvpMatrix * in_Position;
	vTexCoord = in_TexCoord;
}