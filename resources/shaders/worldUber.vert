// Uber shader for world geometry

#version 460 core

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec3 VertexColor;
layout (location = 3) in vec2 TexUV;

uniform mat4 CameraMatrix;

uniform mat4 Matrix = mat4(1.0f);
uniform mat4 Scale = mat4(1.0f);

out vec3 Frag_VertexColor;
out vec2 Frag_UV;

out vec3 Frag_CurrentPosition;
out vec3 Frag_VertexNormal;
out mat4 Frag_CamMatrix;
out mat4 Frag_ModelMatrix;

void main()
{
	Frag_CurrentPosition = vec3(Matrix * Scale * vec4(VertexPosition, 1.0f));
	Frag_VertexNormal = VertexNormal;
	Frag_VertexColor = VertexColor;
	Frag_CamMatrix = CameraMatrix;
	// i cant remember WHAT causes this to be necessary, but it is.
	Frag_UV = mat2(0.0f, -1.0f, 1.0f, 0.0f) * TexUV;

	Frag_ModelMatrix = Matrix;

	gl_Position = CameraMatrix * vec4(Frag_CurrentPosition, 1.0f);
}