#version 460 core

layout (location = 0) in vec2 VertexPosition;
layout (location = 1) in vec2 UVCoordinate;

out vec2 FragIn_UV;

void main()
{
	gl_Position = vec4(VertexPosition.x, VertexPosition.y, 0.0f, 1.0f);
	FragIn_UV = UVCoordinate;
}
