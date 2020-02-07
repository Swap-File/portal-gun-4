uniform mat4 rotatematrix;
attribute vec4 coord;
varying vec2 texpos;
void main(void)
{
	gl_Position = rotatematrix * vec4(coord.xy, 0.0, 1.0);
	texpos = coord.zw;
}