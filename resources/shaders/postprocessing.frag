// This is used to shade the screen just before presenting it
// You can write post-processing effects here - if you're that homosexual :3c
// ...
// Wait...

#version 460 core

in vec2 Frag_UV;

out vec4 FragColor;

uniform sampler2D Phoenix_FramebufferTexture;

uniform bool Phoenix_PostFxEnabled = false;
// uniform sampler2D Phoenix_DistortionTexture;
//uniform sampler2D Phoenix_BloomTexture;
uniform bool Phoenix_ScreenEdgeBlurEnabled = false;
// uniform bool Phoenix_DistortionEnabled = false;

uniform float BlurVignetteStrength = 2.f;
uniform float BlurVignetteDistMul = 2.5f;
uniform float BlurVignetteDistExp = 16.f;
uniform int BlurVignetteSampleRadius = 4;

uniform float Phoenix_Gamma = 1.f;
uniform float Phoenix_Time = 0.f;
const vec2 Center = vec2(0.5f, 0.5f);
const vec3 White = vec3(1.0f, 1.0f, 1.0f);

float roundTo(float n, float to)
{
	return floor((n / to) + 0.5f) * to;
}

// https://stackoverflow.com/a/14081377/16875161
float log10(float x)
{
	return (1 / log(10)) * log(x);
}

void main()
{
	if (!Phoenix_PostFxEnabled)
	{
		FragColor = texture(Phoenix_FramebufferTexture, Frag_UV);
		FragColor.rgb = pow(FragColor.rgb, vec3(1.f / Phoenix_Gamma));
		return;
	}

	ivec2 TextureSize = textureSize(Phoenix_FramebufferTexture, 0);

	// the size of the pixel relative to the screen
	vec2 PixelScale = vec2(1.0f / TextureSize.x, 1.0f / TextureSize.y);

	vec2 UVOffset = vec2(0.f, 0.f);

	vec2 sampleUV = Frag_UV + UVOffset;

	if (sampleUV.x > 1.f || sampleUV.x < 0.f || sampleUV.y > 1.f || sampleUV.y < 0.f)
	{
		FragColor = vec4(0.f, 1.f, 0.f, 1.f);
		return;
	}

	vec2 actualSamplePixel = ivec2(sampleUV * TextureSize);
	vec3 Color = texture(Phoenix_FramebufferTexture, sampleUV).xyz;

	float gamma = Phoenix_Gamma;

	/*if (Time > 10.f)
	{
	        float t = Time - 5.f;
		Color *= mix(vec3(1.f, 1.f, 1.f), vec3(3.f, 0.1f, 0.1f), clamp((t - 5.f) / 10, 0.f, 1.f));
		gamma = mix(1.f, 1.5f, clamp(t - 10.f, 0.f, t) / 10.f);
	}*/

	//Color += texture(BloomTexture, sampleUV).xyz;

	if (Phoenix_ScreenEdgeBlurEnabled)
	{
		vec3 BlurredColor;

		// Multiply to create wider region of blur,
		// then exponent to widen the 0% and make the 
		// transition steeper
		float RadialBlurWeight = clamp(pow(length(Frag_UV - Center), BlurVignetteDistExp) * BlurVignetteDistMul, 0.f, 1.f);

		int BlurSampleRadius = BlurVignetteSampleRadius;

		float BlurSampleBaseWeight = 1.f/(BlurSampleRadius * BlurSampleRadius);

		for (int x = -BlurSampleRadius; x < BlurSampleRadius; x++)
		{
			for (int y = -BlurSampleRadius; y < BlurSampleRadius; y++)
			{
				float Dist = length(vec2(x, y) * BlurVignetteStrength) / (BlurSampleRadius * BlurVignetteStrength);

				float DistFactor = 1.f - Dist;
				float SampleWeight = pow(DistFactor * 1.f, 1.f) * BlurSampleBaseWeight;

				vec3 SampleCol = texture(Phoenix_FramebufferTexture, Frag_UV + (vec2(x, y) * PixelScale) + UVOffset).xyz;
				BlurredColor += SampleCol * SampleWeight;
			}
		}
		
		Color = mix(Color, BlurredColor, RadialBlurWeight);
	}

	// Reinhardt extended
	/*
	const float WhitePoint = 2.5f;

	float luminanceI = Color.r * 0.2126f + Color.g * 0.7152f + Color.b * 0.0722f;
	float luminanceO = (luminanceI * (1 + (luminanceI/pow(WhitePoint, 2.f))))/(1+luminanceI);

	Color = Color * (luminanceO / luminanceI);
	*/

	Color = pow(Color, vec3(gamma, gamma, gamma));

	FragColor = vec4(Color, 1.0f);
}
