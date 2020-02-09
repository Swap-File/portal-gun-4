#version 310 es
precision mediump float;
uniform sampler2D u_Texture;
uniform float u_Alpha;
in vec2 vTexCoord;
out vec4 vFragColor;
void main()
{
	vFragColor = texture( u_Texture, vTexCoord ) * vec4(1.0,1.0,1.0,u_Alpha);
}