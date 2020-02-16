#version 310 es
//https://www.shadertoy.com/view/Ms3SWs
precision highp float;
precision highp int;
uniform float u_time;
uniform float u_Alpha;
uniform bool u_blue;
in vec2 uVu;
out vec4 fragColor;
void main(){
    float layer1=sin(u_time*8.54-sin(length(uVu-vec2(-0.09,0.1)))*55.0);
    float layer2=sin(u_time*7.13-sin(length(uVu-vec2(1.5,1.2)))*43.0);
    float layer3=sin(u_time*7.92-sin(length(uVu-vec2(0.11,-0.02)))*42.5);
    float layer4=sin(u_time*6.71-sin(length(uVu-vec2(-.75,-1.0)))*47.2);
    float b=smoothstep(-2.5,10.0,layer1+layer2+layer3+layer4);
    float waveHeight= 0.4+b*3.0;
	fragColor = vec4(waveHeight,waveHeight*0.5,waveHeight*0.3, u_Alpha);
	if (u_blue)
		fragColor = fragColor.zyxw;
}