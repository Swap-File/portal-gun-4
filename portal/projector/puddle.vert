#version 310 es
uniform float u_size;
in vec4 in_Position;
in vec2 in_TexCoord;
out vec2 uVu;
void main()
{
	uVu = vec2(u_size,u_size) * in_TexCoord;
	gl_Position = in_Position;
}