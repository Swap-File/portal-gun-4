#version 310 es
//https://www.shadertoy.com/view/Wt3GRS
precision highp float;
precision highp int;
uniform float u_time;
uniform bool u_blue;
uniform bool u_shutter;
in vec2 p;
out vec4 fragmentColor;

float snoise(vec3 uv, float res)
{
    const vec3 s = vec3(1e0, 1e2, 1e3);
    uv *= res;
    vec3 uv0 = floor(mod(uv, res))*s;
    vec3 uv1 = floor(mod(uv+vec3(1.), res))*s;
    vec3 f = fract(uv);
    f = f*f; //faster
    f *= (3.0-2.0*f); //faster
    vec4 v = vec4(uv0.x+uv0.y+uv0.z, uv1.x+uv0.y+uv0.z,uv0.x+uv1.y+uv0.z, uv1.x+uv1.y+uv0.z);
    vec4 r = fract(sin(v*1e-1)*1e3);
    float r0 = mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y);
    r = fract(sin((v + uv1.z - uv0.z)*1e-1)*1e3);
    float r1 = mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y);
    return mix(r0, r1, f.z)*2.-1.;
}

float atan2_approximation1(vec2 temp)  //1ms faster than atan, looks the same
{
#define M_PI   3.14159265358979323846264338327950288
#define ONEQTR_PI  0.78539816339744830961566084581987572
#define THRQTR_PI 2.35619449019234492884698253745962716
    float r, angle;
    float abs_y = abs(temp.y) + 1e-10f;      // kludge to prevent 0/0 condition
    if ( temp.x < 0.0f )
    {
        r = (temp.x + abs_y) / (abs_y - temp.x);
        angle = THRQTR_PI;
    }
    else
    {
        r = (temp.x - abs_y) / (temp.x + abs_y);
        angle = ONEQTR_PI;
    }
    float temp2 = r * r;  //faster to have this on its own line
    angle += (0.1963f * temp2 - 0.9817f) * r;
    if ( temp.y < 0.0f )
        return( -angle );  // negate if in quad III or IV
    else
        return( angle );
}

void main()
{
    if (u_shutter) {
        fragmentColor = vec4(0.0,0.0,0.0,1.0);
    } else {
        float color = 3.0 - (3.*length(2.*p));
        vec3 coord = 1.0 - vec3(atan2_approximation1(p)*(1.0/6.2832)+.5, length(p)*.4, .5);
        const float power = 2.0;
        color += (0.4 / power) * snoise(coord + vec3(0,-u_time*.05, u_time*.01), power*32.);
        color = 1.0 - color;
        color *= 2.7;
        color *= smoothstep(0.43, 0.4, length(p));
        float alpha = smoothstep(0.33,0.43, length(p));
        fragmentColor = vec4(color, pow(max(color,0.),2.)*0.4, pow(max(color,0.),3.)*0.15,max(alpha,color));
        if (u_blue)
            fragmentColor = fragmentColor.zyxw;
    }
}