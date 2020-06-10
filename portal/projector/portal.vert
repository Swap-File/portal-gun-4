#version 310 es
uniform float u_size;
in vec4 in_Position;
in vec2 in_TexCoord;
out vec2 p;
void main()
{
    p = u_size * (-1.0 + 2.0 * in_TexCoord);
    gl_Position = in_Position;
}