#version 310 es
precision mediump float;
uniform sampler2D s_texture;
in vec2 vTexCoord;
out vec4 fragmentColor;
void main()
{
	fragmentColor = texture2D( s_texture, vTexCoord );
}