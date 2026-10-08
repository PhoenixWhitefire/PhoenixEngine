// This is used to shade the screen just before presenting it
// You can write post-processing effects here - if you're that homosexual :3c
// ...
// Wait...

#version 460 core

in vec2 Frag_UV;

out vec4 FragColor;

uniform sampler2D Phoenix_PostProcessBuffer;
uniform sampler2D Phoenix_BloomResult;

uniform bool Phoenix_PostFxEnabled = false;
uniform bool Phoenix_BloomEnabled = false;
uniform bool Phoenix_ScreenEdgeBlurEnabled = false;

uniform float Phoenix_Gamma = 1.f;
uniform float Phoenix_Time = 0.f;

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
		FragColor = texture(Phoenix_PostProcessBuffer, Frag_UV);
		FragColor.rgb = pow(FragColor.rgb, vec3(1.f / Phoenix_Gamma));
		return;
	}

	ivec2 TextureSize = textureSize(Phoenix_PostProcessBuffer, 0);

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
	vec3 Color = texture(Phoenix_PostProcessBuffer, sampleUV).xyz;

	if (Phoenix_BloomEnabled)
	{
		Color += texture(Phoenix_BloomResult, sampleUV).xyz;
	}

	// Reinhardt extended
	/*
	const float WhitePoint = 2.5f;

	float luminanceI = Color.r * 0.2126f + Color.g * 0.7152f + Color.b * 0.0722f;
	float luminanceO = (luminanceI * (1 + (luminanceI/pow(WhitePoint, 2.f))))/(1+luminanceI);

	Color = Color * (luminanceO / luminanceI);
	*/

	Color = pow(Color, vec3(1.f / Phoenix_Gamma));

	FragColor = vec4(Color, 1.0f);
}
