// Uber shader for world geometry

// TODO: Remove unnecessary variables
// - 21/06/2024

#version 460 core
#extension GL_ARB_shading_language_include : require

#include "/include/worldCommon.frag"

in vec3 Frag_ModelPosition;
in vec3 Frag_WorldPosition;
in vec3 Frag_VertexNormal;
in vec4 Frag_Paint;
in vec2 Frag_TextureUV;
in mat4 Frag_Transform;
in vec4 Frag_RelativeToDirecLight;
//in mat3 Frag_TBN;
in vec3 Frag_CameraPosition;

out vec4 FragColor;

void main()
{
	if (Frag_TextureUV.x < 0.02f || Frag_TextureUV.y < 0.02f || Frag_TextureUV.x > 0.98f || Frag_TextureUV.y > 0.98f)
		FragColor = texture(Phoenix_Material.ColorMap, Frag_TextureUV) * Frag_Paint;
	else
		discard;
}
