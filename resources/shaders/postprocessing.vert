#version 460 core

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal; // unused
layout (location = 2) in vec3 VertexColor; // unused
layout (location = 3) in vec2 TexUV;

uniform vec3 Scale;

out vec2 FragIn_UV;

void main()
{
	gl_Position = vec4(VertexPosition.x * Scale.x, VertexPosition.y * Scale.y, 0.f, 1.0f);
	FragIn_UV = vec2(TexUV.x, 1.f - TexUV.y);
}
