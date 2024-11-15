// This is used to shade the screen just before presenting it
// You can write post-processing effects here - if you're that rampantly homosexual :3c
// ...
// Wait...

#version 460 core

in vec2 FragIn_UV;

out vec4 FragColor;

uniform sampler2D Texture;

uniform bool PostFxEnabled = false;
uniform sampler2D DistortionTexture;
uniform bool ScreenEdgeBlurEnabled = false;
uniform bool DistortionEnabled = false;

uniform float BlurVignetteStrength = 2.f;
uniform float BlurVignetteDistMul = 2.5f;
uniform float BlurVignetteDistExp = 16.f;
uniform int BlurVignetteSampleRadius = 4;

uniform float Gamma = 1.f;
uniform float LdMax = .5f;
uniform float ContrastMax = .9f;

uniform float Time = 0.f;

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

float getBrightness(vec3 Value)
{
	return dot(Value, vec3(0.299f, 0.587f, 0.114f));
}

void main()
{
	if (!PostFxEnabled)
	{
		FragColor = texture(Texture, FragIn_UV);
		return;
	}

	ivec2 TextureSize = textureSize(Texture, 0);

	// the size of the pixel relative to the screen
	vec2 PixelScale = vec2(1.0f / TextureSize.x, 1.0f / TextureSize.y);

	vec2 UVOffset = vec2(0.f, 0.f);

	if (DistortionEnabled)
		UVOffset = (texture(DistortionTexture, FragIn_UV).xy - Center) * (sin(Time) * 5);

	vec2 sampleUV = FragIn_UV + UVOffset;
	vec2 actualSamplePixel = ivec2(sampleUV * TextureSize);
	vec3 Color = texture(Texture, sampleUV).xyz;

	for (int i = 1; i < 7; i++)
	{
		vec3 colDownscaled = textureLod(Texture, sampleUV, i).xyz;
		float brightness = getBrightness(colDownscaled);

		if (brightness > 1.2f)
			Color += colDownscaled / pow(i, 2);
	}

	// https://youtu.be/wbn5ULLtkHs?t=271
	float Lin = getBrightness(Color);
	float Lavg = getBrightness(textureLod(Texture, sampleUV, 10).xyz);

	float logLrw = log10(Lavg) + 0.84f;
	float alphaRw = 0.4f * logLrw + 2.92f;
	float betaRw = -0.4f * logLrw * logLrw - 2.584f * logLrw + 2.0208f;
	float Lwd = LdMax / sqrt(ContrastMax);
	float logLd = log10(Lwd) + 0.84f;
	float alphaD = 0.4f * logLd + 2.92f;
	float betaD = -0.4f * logLd * logLd - 2.584f * logLd + 2.0208f;
	float Lout = pow(Lin, alphaRw / alphaD) / LdMax * pow(10.f, (betaRw - betaD) / alphaD) - (1.f - ContrastMax);

	Color = Color / Lin * Lout;
	Color *= 0.6f;

	if (ScreenEdgeBlurEnabled)
	{
		vec3 BlurredColor;

		// Multiply to create wider region of blur,
		// then exponent to widen the 0% and make the 
		// transition steeper
		float RadialBlurWeight = clamp(pow(length(FragIn_UV - Center), BlurVignetteDistExp) * BlurVignetteDistMul, 0.f, 1.f);

		int BlurSampleRadius = BlurVignetteSampleRadius;

		float BlurSampleBaseWeight = 1.f/(BlurSampleRadius * BlurSampleRadius);

		for (int x = -BlurSampleRadius; x < BlurSampleRadius; x++)
		{
			for (int y = -BlurSampleRadius; y < BlurSampleRadius; y++)
			{
				float Dist = length(vec2(x, y) * BlurVignetteStrength) / (BlurSampleRadius * BlurVignetteStrength);

				float DistFactor = 1.f - Dist;
				float SampleWeight = pow(DistFactor * 1.f, 1.f) * BlurSampleBaseWeight;

				vec3 SampleCol = texture(Texture, FragIn_UV + (vec2(x, y) * PixelScale) + UVOffset).xyz;
				BlurredColor += SampleCol * SampleWeight;
			}
		}
		
		Color = mix(Color, BlurredColor, RadialBlurWeight);
	}

	Color = pow(Color, vec3(Gamma, Gamma, Gamma));
	
	FragColor = vec4(Color, 1.0f);
}
