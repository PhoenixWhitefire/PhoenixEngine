// Skybox shader

#version 460 core

layout (location = 0) in vec3 VertexPosition;

out vec3 FragIn_Direction;

uniform mat4 RenderMatrix;

void main()
{
	vec4 Position = RenderMatrix * vec4(VertexPosition, 1.0f);

	gl_Position = Position.xyww;

	FragIn_Direction = VertexPosition.xyz;
}
