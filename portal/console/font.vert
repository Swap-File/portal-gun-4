#version 310 es
uniform mat4 rotatematrix;
in vec4 coord;
out vec2 texpos;
void main(void)
{
	gl_Position = rotatematrix * vec4(coord.xy, 0.0, 1.0);
	texpos = coord.zw;
}