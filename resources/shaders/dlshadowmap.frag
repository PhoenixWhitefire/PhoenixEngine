// ShadowMap shader for Directional Lights

#version 460 core

const int MAX_DIFFUSE_TEXTURES = 6;
const int MAX_SPECULAR_TEXTURES = 6;

uniform sampler2D DiffuseTextures[MAX_DIFFUSE_TEXTURES];

uniform int NumDiffuseTextures = 1;

in vec2 UVCoords;

void main()
{
	/*float Alpha = 1.0f;

	for (int dtex = 0; dtex < NumDiffuseTextures; dtex++)
	{
		float mipLevel = textureQueryLod(DiffuseTextures[dtex], UVCoords).x;
		float talpha = textureLod(DiffuseTextures[dtex], UVCoords, mipLevel).w;

		if (talpha < Alpha)
			Alpha = talpha;
	}

	if (Alpha < .1f)
		discard;*/
}
