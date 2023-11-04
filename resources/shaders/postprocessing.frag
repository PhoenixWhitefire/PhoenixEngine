#version 330 core

in vec2 FragIn_UV;

out vec4 FragColor;

uniform sampler2D Texture;
//uniform sampler2D DistortionTexture;

const vec3 White = vec3(1.0f, 1.0f, 1.0f);

// the offsets of the pixels we are using to get the color of the current pixel (0, 0)

vec2 offsets[9] = vec2[]
(
    vec2(-1,  1), vec2( 0.0f,    1), vec2( 1,  1),
    vec2(-1,  0.0f),     vec2( 0.0f,    0.0f),     vec2( 1,  0.0f),
    vec2(-1, -1), vec2( 0.0f,   -1), vec2( 1, -1) 
);

float ninth = .1;

float kernel[9] = float[]
(
	ninth, ninth, ninth,
	ninth, ninth, ninth,
	ninth, ninth, ninth
);

void main()
{
	ivec2 TextureSize = textureSize(Texture, 0);

	// the size of the pixel relative to the screen
	vec2 PixelScale = vec2(1.0f / TextureSize.x, 1.0f / TextureSize.y);
/*
	vec3 Color = vec3(texelFetch(
		Texture,
		ivec2(FragIn_UV.st * TextureSize),// + texture(DistortionTexture, FragIn_UV).xy),
		0
	));
	*/

	bool ScreenEdgeBlur = false;

	vec3 Color = texture(Texture, FragIn_UV).xyz;

	if (ScreenEdgeBlur)
	{
		vec3 BlurredColor;

		float RadialBlurWeight = length((FragIn_UV - vec2(0.5, 0.5)) * 2);

		int BlurSampleRadius = 4;

		float BlurSize = 1;

		float BlurSampleWeight = 1.f/(float(BlurSampleRadius) * float(BlurSampleRadius));

		for (int x = -BlurSampleRadius; x < BlurSampleRadius; x++)
		{
			for (int y = -BlurSampleRadius; y < BlurSampleRadius; y++)
			{
				BlurredColor += texture(Texture, FragIn_UV + (vec2(x, y) * PixelScale * BlurSize)).xyz * BlurSampleWeight / (BlurSampleRadius);
			}
		}

		// Linear interpolation
		Color = Color + (BlurredColor - Color) * RadialBlurWeight;
	}
	
	FragColor = vec4(Color, 1.0f);
}
