// Uber shader for world geometry

#version 460 core

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec3 VertexColor;
layout (location = 3) in vec2 TexUV;
// from Instanced Array
//layout (location = 4) in mat4 InstanceTransform;

uniform mat4 CameraMatrix;

uniform mat4 Transform;
uniform mat4 Scale;

uniform float Time;

out vec3 Frag_CurrentPosition;
out vec3 Frag_VertexNormal;
out vec3 Frag_VertexColor;
out vec2 Frag_UV;
out mat4 Frag_CamMatrix;
out mat4 Frag_Transform;

void main()
{
	Frag_CurrentPosition = vec3(Transform * Scale * vec4(VertexPosition, 1.0f));
	Frag_VertexNormal = VertexNormal;
	Frag_VertexColor = VertexColor;
	Frag_CamMatrix = CameraMatrix;
	// the UVs need to be flipped 90 degrees because of some
	// undefined-behavior with how Mr Victor Gordan's excellent Model Importing
	// code groups floats
	// 10/11/2024
	Frag_UV = mat2(0.0, -1.0, 1.0, 0.0) * TexUV;
	Frag_Transform = Transform;
	
	gl_Position = Frag_CamMatrix * vec4(Frag_CurrentPosition, 1.f);
}
