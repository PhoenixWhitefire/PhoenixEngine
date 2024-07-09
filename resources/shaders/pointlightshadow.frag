#version 330 core

in vec4 FragPos;

uniform vec3 LightPos;
uniform float FarZ;

void main()
{
	gl_FragDepth = length(FragPos.xyz - LightPos) / FarZ;
}
