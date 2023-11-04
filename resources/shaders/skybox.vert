#version 330 core

layout (location = 0) in vec3 VertexPosition;

out vec3 FragIn_Direction;

uniform mat4 ProjectionMat;
uniform mat4 ViewMat;

void main()
{
	vec4 Position = ProjectionMat * ViewMat * vec4(VertexPosition, 1.0f);

	gl_Position = Position.xyww;

	FragIn_Direction = VertexPosition.xyz;
}
