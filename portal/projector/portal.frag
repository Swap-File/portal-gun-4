#version 310 es
precision highp float;
precision highp int;
uniform float u_time;
uniform float u_size;
uniform bool u_blue;
uniform bool u_shutter;
in vec2 vUv;
out vec4 fragmentColor;
float snoise(vec3 uv, float res)
{
	const vec3 s = vec3(1e0, 1e2, 1e3);
	uv *= res;
	vec3 uv0 = floor(mod(uv, res))*s;
	vec3 uv1 = floor(mod(uv+vec3(1.), res))*s;
	vec3 f = fract(uv); f = f*f*(3.0-2.0*f);
	vec4 v = vec4(uv0.x+uv0.y+uv0.z, uv1.x+uv0.y+uv0.z,uv0.x+uv1.y+uv0.z, uv1.x+uv1.y+uv0.z);
	vec4 r = fract(sin(v*1e-1)*1e3);
	float r0 = mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y);
	r = fract(sin((v + uv1.z - uv0.z)*1e-1)*1e3);
	float r1 = mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y);
	return mix(r0, r1, f.z)*2.-1.;
}
void main()
{
	if (u_shutter){
		fragmentColor = vec4(0.0,0.0,0.0,1.0);
	} else {
		vec2 p =  u_size * (-1.0 + 2.0 * vUv);  
		float color = 3.0 - (3.*length(2.*p));
		vec3 coord = vec3(atan(p.x,p.y)*(1.0/6.2832)+.5, length(p)*.4, .5);  
		coord = 1.0 - coord;
		const float power = 2.0;
		color += (0.4 / power) * snoise(coord + vec3(0,-u_time*.05, u_time*.01), power*32.);
		color = 1.0 - color;
		color *= 2.7;
		color *= smoothstep(0.43, 0.4, length(p));
		float alpha = smoothstep(0.25 ,0.44, length(p));
		fragmentColor = vec4(pow(max(color,0.),3.)*0.15, pow(max(color,0.),2.)*0.4, color, alpha);
		if (u_blue) //swizzling is so fast its not measurable
			fragmentColor = fragmentColor.zyxw;
	}
}