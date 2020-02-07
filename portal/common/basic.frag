#version 310 es
precision mediump float;
uniform sampler2D u_Texture;
in vec2 vTexCoord;
out vec4 vFragColor;
void main()
{
	vFragColor = texture( u_Texture, vTexCoord );
}