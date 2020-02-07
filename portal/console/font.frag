precision mediump float;
varying vec2 texpos;
uniform sampler2D tex;
uniform vec4 color;
void main(void)
{
	gl_FragColor = vec4(1.0, 1.0, 1.0, texture2D(tex, texpos).a) * color;
}