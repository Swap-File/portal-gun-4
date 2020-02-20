#version 310 es

in vec3 vertex;
in vec4 in_color;
out vec4 v_color;

void main()
{
    gl_Position = vec4(vertex.xy,0.0,1.0);
	gl_PointSize = vertex.z; 
	v_color = in_color;
}