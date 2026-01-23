// Uber shader for world geometry

// TODO: Remove unnecessary variables
// - 21/06/2024

#version 460 core

uniform bool IsShadowMap;

in vec3 Frag_ModelPosition;
in vec3 Frag_WorldPosition;
in vec3 Frag_VertexNormal;
in vec4 Frag_Paint;
in vec2 Frag_TextureUV;
in mat4 Frag_Transform;
in vec4 Frag_RelativeToDirecLight;
//in mat3 Frag_TBN;
in vec3 Frag_CameraPosition;
in float Frag_Transparency;

out vec4 FragColor;

void main()
{
	if (IsShadowMap)
		discard;

	FragColor = vec4(Frag_Paint.xyz, 1.f - Frag_Transparency);

	if (Frag_Transparency > 0.f)
	{
		FragColor = vec4(Frag_Paint.xyz, 1.f - Frag_Transparency);
		gl_FragDepth = 0;
	}
}
