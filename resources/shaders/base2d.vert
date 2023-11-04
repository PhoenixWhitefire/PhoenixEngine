#version 460 core

layout (location = 0) in vec2 VertexPosition;
layout (location = 1) in vec3 VertexColor;
layout (location = 2) in vec2 VertexUV;

out vec3 FragIn_VertexColor;
out vec2 FragIn_UV;

uniform vec2 Position;
uniform float Scale;
uniform float Time;

void main(){
	gl_Position = vec4(Position.x + VertexPosition.x * Scale, Position.y + VertexPosition.y * Scale, 0.0f, 1.0f);

	FragIn_VertexColor = VertexColor;
	FragIn_UV = VertexUV;
}
