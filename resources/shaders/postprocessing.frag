#version 330 core

in vec2 FragIn_UV;

out vec4 FragColor;

uniform sampler2D Texture;
uniform sampler2D DistortionTexture;
uniform bool PostProcessingEnabled = false;
uniform bool ScreenEdgeBlurEnabled = false;
uniform bool DistortionEnabled = false;

uniform float Time = 0.f;

const vec2 Center = vec2(0.5f, 0.5f);

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

	vec2 UVOffset = vec2(0.f, 0.f);

	if (DistortionEnabled)
	{
		UVOffset = texture(DistortionTexture, FragIn_UV).xy - Center + vec2(Time * 0.5f, Time * 0.25f);
	}

	//FragColor = vec4(texture(DistortionTexture, FragIn_UV).xyz, 1.0f);

	//return;

	vec3 Color = texture(Texture, FragIn_UV + UVOffset).xyz;

	//Color = vec3(1.0f, 0.0f, 0.0f);

	if (ScreenEdgeBlurEnabled)
	{
		const float BlurStrength = 2.f;
		
		vec3 BlurredColor;

		// Multiply to create wider region of blur,
		// then exponent to widen the 0% and make the 
		// transition steeper
		float RadialBlurWeight = clamp(pow(length(FragIn_UV - Center), 2.5f) * 16.f, 0.f, 1.f);

		//FragColor = vec4(RadialBlurWeight, RadialBlurWeight, RadialBlurWeight, 1.f);

		//return;

		int BlurSampleRadius = 4;

		float BlurSampleBaseWeight = 1.f/(BlurSampleRadius * BlurSampleRadius);

		for (int x = -BlurSampleRadius; x < BlurSampleRadius; x++)
		{
			for (int y = -BlurSampleRadius; y < BlurSampleRadius; y++)
			{
				float Dist = length(vec2(x, y) * BlurStrength) / (BlurSampleRadius * BlurStrength);

				float DistFactor = 1.f - Dist;
				float SampleWeight = pow(DistFactor * 1.f, 1.f) * BlurSampleBaseWeight;

				vec3 SampleCol = texture(Texture, FragIn_UV + (vec2(x, y) * PixelScale)).xyz;
				BlurredColor += SampleCol * SampleWeight;
			}
		}

		// Linear interpolation
		Color = Color + (BlurredColor - Color) * RadialBlurWeight;
	}
	
	FragColor = vec4(Color * 1.25f, 1.0f);
}
