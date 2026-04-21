// Uber shader for world geometry

// TODO: Remove unnecessary variables
// - 21/06/2024

#version 460 core
#extension GL_ARB_shading_language_include : require

#include "/include/worldCommon.frag"

in vec3 Frag_ModelPosition;
in vec3 Frag_WorldPosition;
in vec3 Frag_VertexNormal;
in vec3 Frag_ColorTint;
in vec2 Frag_TextureUV;
in mat4 Frag_Transform;
in vec4 Frag_RelativeToDirecLight;
//in mat3 Frag_TBN;
in vec3 Frag_CameraPosition;

out vec4 FragColor;

void main()
{
	// Bright magenta
	FragColor = vec4(1.f, 0.f, 1.f, 1.f);
}
