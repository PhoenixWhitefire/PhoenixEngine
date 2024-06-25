// ShadowMap shader for Directional Lights

#version 460 core

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec3 VertexColor;
layout (location = 3) in vec2 TexUV;

uniform mat4 LightProjection;

out vec2 UVCoords;

uniform mat4 Matrix = mat4(1.0f);
uniform mat4 Scale = mat4(1.0f);

void main()
{
	vec3 FragIn_CurrentPosition = vec3(Matrix * Scale * vec4(VertexPosition, 1.0f));
	UVCoords = mat2(0.0f, -1.0f, 1.0f, 0.0f) * TexUV;

	gl_Position = LightProjection * vec4(FragIn_CurrentPosition, 1.0f);
}
