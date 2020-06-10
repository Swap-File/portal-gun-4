#version 310 es

precision highp float;
precision highp int;

uniform sampler2D u_Texture;

in vec4 v_color;
out vec4 color;

void main()  //gl_PointCoord
{
    color = v_color * texture( u_Texture, gl_PointCoord );
}