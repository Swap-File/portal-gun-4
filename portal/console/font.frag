#version 310 es
precision mediump float;
in vec2 texpos;
uniform sampler2D tex;
uniform vec4 color;
out vec4 fragmentColor;
void main(void)
{
    fragmentColor = vec4(1.0, 1.0, 1.0, texture(tex, texpos).a) * color;
}