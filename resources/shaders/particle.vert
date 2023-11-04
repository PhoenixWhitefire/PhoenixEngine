#version 330 core

layout (location = 0) in vec2 VertexPosition;

uniform mat4 CameraMatrix = mat4(1.0f);
uniform vec3 CameraPosition;

uniform mat4 Scale = mat4(1.0f);

uniform vec3 Position;

out vec2 FragIn_VertexPosition;

void main()
{
	//vec3 CurrentPosition = vec3(Scale * vec4(VertexPosition.x, VertexPosition.y, 0.0f, 1.0f));
	
	//gl_Position = CameraMatrix * vec4(CurrentPosition, 1.0f);

	vec4 ViewPosition = CameraMatrix * vec4(Position * 5.0f, 1.0f);

	float Distance = -Position.z * 50.0f;

	gl_Position = vec4(1.0f, 0.5f, 0.5f, 1.0f);

	//gl_Position = ViewPosition + vec4(VertexPosition.xy * Distance * vec3(Scale).x, 0.0f, 0.0f);

	FragIn_VertexPosition = vec2(1.0f, 0.5f);//VertexPosition;
}
