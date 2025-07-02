// Uber shader for world geometry

// TODO: Remove unnecessary variables
// - 21/06/2024

#version 460 core

const int MAX_LIGHTS = 6;

/*
LIGHT TYPE IDS:
-Directional lights = 0
-Point lights = 1
-Spot lights = 2

When Directional light, Position = Direction
*/
struct LightObject
{
	int Type;
	bool Shadows;

	// generic, applies to all light types (i.e., directional, point, spot lights)
	vec3 Position;
	vec3 Color;

	// point lights and spotlights
	float Range;
	// spotlights
	float Angle;
};

uniform LightObject Lights[MAX_LIGHTS];

uniform sampler2D ShadowAtlas;

uniform int NumLights;

uniform sampler2D FrameBuffer;

uniform sampler2D ColorMap;
uniform sampler2D MetallicRoughnessMap;
uniform bool HasNormalMap;
uniform sampler2D NormalMap;
uniform bool HasEmissionMap;
uniform sampler2D EmissionMap;

uniform vec3 LightAmbient = vec3(0.3f);
uniform float SpecularMultiplier;
uniform float SpecularPower;

uniform float MetallnessFactor;
uniform float RoughnessFactor;
uniform float EmissionStrength;

uniform samplerCube SkyboxCubemap;

uniform float NearZ = 0.1f;
uniform float FarZ = 1000.0f;

uniform bool Fog;
uniform vec3 FogColor = vec3(0.85f, 0.85f, 0.90f);

uniform bool UseTriPlanarProjection;
uniform float MaterialProjectionFactor;

uniform float AlphaCutoff = 0.5f;

uniform bool IsShadowMap;
uniform bool DebugOverdraw = false;
uniform bool DebugLightInfluence = false;

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

float PointLight(vec3 Direction, float Range)
{
	float Distance = length(Direction);

	/*
	float a = 3.0f;
	float b = 0.7f;

	float Intensity = 1.0f / (a * Distance * Distance + b * Distance + 1.0f);
	*/

	// Range >= 0, linear attenuation
	// Range < 0, realistic attenuation
	if (Range >= 0.f)
		return 1 - clamp(Distance/Range, 0.0f, 1.0f);
	else
		return 1.f / (pow(Distance, 2.f));
}

float SpotLight(vec3 Direction, float OuterCone, float InnerCone)
{
	float Angle = dot(vec3(0.0f, -1.0f, 0.0f), -normalize(Direction));

	return clamp(Angle + (OuterCone / InnerCone) * (Angle * OuterCone), 0.0f, 1.0f);
}

float LinearizeDepth(float d)
{
	return (2.0f * NearZ * FarZ) / (FarZ + NearZ - (d * 2.0f - 1.0f) * (FarZ - NearZ));
}

float LogisticDepth(float Steepness, float Offset)
{
	float ZVal = LinearizeDepth(gl_FragCoord.z);

	return (1 / (1 + exp(-Steepness * (ZVal - Offset))));
}

//Calculates what a light would add to a pixel
vec3 CalculateLight(int Index, vec3 Normal, vec3 Outgoing, float SpecMapValue)
{
	LightObject Light = Lights[Index];

	vec3 LightPosition = Light.Position;
	vec3 LightColor = Light.Color;

	int LightType = Light.Type;
	
	if (LightType == 0)
	{
		vec3 Incoming = normalize(LightPosition);
		
		float shadow = 0.f;
		
		if (Light.Shadows)
		{
			vec3 lightCoords = Frag_RelativeToDirecLight.xyz / Frag_RelativeToDirecLight.w;
			if (lightCoords.z <= 1.f)
			{
				lightCoords = (lightCoords + 1.f) / 2.f;
				float currentDepth = lightCoords.z;

				//float curDepthL = LinearizeDepth(curDepth);
				float bias = max(0.025f * (1.f - dot(Normal, Incoming)), 0.4f);
				
				const int SampleRadius = 1;
				vec2 pixelSize = 1.f / textureSize(ShadowAtlas, 0);
				for (int y = -SampleRadius; y <= SampleRadius; y++)
					for (int x = -SampleRadius; x <= SampleRadius; x++)
					{
						float closestDepth = texture(ShadowAtlas, lightCoords.xy + vec2(x, y) * pixelSize).r;
						if (currentDepth > closestDepth + bias)
							shadow += 1.f;
					}

				shadow /= pow((SampleRadius * 2 + 1), 2);
			}
		}
		//shadow = 0.f;

		//return vec3(1.f - shadow);

		float Intensity = max(dot(Normal, Incoming), 0.f);
		float Diffuse = Intensity;

		float Specular = 0.f;

		if (Diffuse > 0.f && shadow == 0.f)
		{
			vec3 reflectDir = reflect(-Incoming, Normal);
			Specular = pow(max(dot(Outgoing, reflectDir), 0.f), SpecularPower);
		}

		float SpecularTerm = SpecMapValue * Specular * SpecularMultiplier;

		return (Diffuse * (1.f - shadow) + SpecularTerm * (1.f - shadow) * Intensity) * LightColor;
		//FinalColor = Albedo * SpecMapValue;
	}
	else if (LightType == 1)
	{
		vec3 LightToPosition = LightPosition - Frag_WorldPosition;
		vec3 Incoming = normalize(LightToPosition);

		float Diffuse = max(dot(Normal, Incoming), 0.0f);

		float Specular = 0.0f;

		if (Diffuse > 0.0f)
		{
			//vec3 Halfway = normalize(ViewDirection + LightDirection);

			vec3 reflectionVector = reflect(-Incoming, Normal);

			Specular = pow(max(dot(Outgoing, reflectionVector), 0.f), SpecularPower);
		}

		float Intensity = PointLight(LightToPosition, Light.Range);
		float SpecularTerm = SpecMapValue * Specular * SpecularMultiplier;

		return ((Diffuse * Intensity) + SpecularTerm * Intensity) * LightColor;
	}
	else
	{
		return vec3(1.0f, 0.0f, 1.0f);
	}
}

// from: https://gist.github.com/patriciogonzalezvivo/20263fe85d52705e4530
vec3 getTriPlanarBlending(vec3 _wNorm)
{
	// in wNorm is the world-space normal of the fragment
	vec3 blending = abs( _wNorm );
	blending = normalize(max(blending, 0.00001)); // Force weights to sum to 1.0
	float b = (blending.x + blending.y + blending.z);
	blending /= vec3(b, b, b);
	return blending;
}

void main()
{
	float mipLevel = textureQueryLod(ColorMap, Frag_TextureUV).x;

	if (IsShadowMap)
	{
		if (textureLod(ColorMap, Frag_TextureUV, mipLevel).w < AlphaCutoff)
			discard;

		FragColor = vec4(gl_FragCoord.z * 0.25f, gl_FragCoord.z * 0.25f, gl_FragCoord.z * 0.25f, 1.f);

		return;
	}

	// Convert mesh normals to world-space
	mat3 NormalMatrix = transpose(inverse(mat3(Frag_Transform)));

	vec3 vertexNormal = Frag_VertexNormal;

	vec3 ViewDirection = normalize(Frag_CameraPosition - Frag_WorldPosition);

	vec4 Albedo = vec4(0.f, 0.f, 0.f, 1.f); //textureLod(ColorMap, UV, mipLevel);
	vec2 MetallicRoughnessSample = vec2(0.f, 1.f);
	vec3 EmissionSample = vec3(1.f, 1.f, 1.f);
	vec3 NormalSample;

	if (!UseTriPlanarProjection)
	{
		MetallicRoughnessSample = textureLod(MetallicRoughnessMap, Frag_TextureUV, mipLevel).rg;
		Albedo = textureLod(ColorMap, Frag_TextureUV, mipLevel);

		if (HasNormalMap)
			NormalSample = (textureLod(NormalMap, Frag_TextureUV, mipLevel).xyz - vec3(0.f, 0.f, 1.f)) * 2.f - 1.f;

		if (HasEmissionMap)
			EmissionSample = textureLod(EmissionMap, Frag_TextureUV, mipLevel).xyz;
	}
	else
	{
		vec3 blending = getTriPlanarBlending(Frag_VertexNormal);
		vec2 uvXAxis = Frag_ModelPosition.zy * vec2(1.f, -1.f) * MaterialProjectionFactor;
		vec2 uvYAxis = Frag_ModelPosition.xz * vec2(-1.f, 1.f) * MaterialProjectionFactor;
		vec2 uvZAxis = -Frag_ModelPosition.xy * MaterialProjectionFactor;
		
		vec4 xAxis = textureLod(ColorMap, uvXAxis, mipLevel);
		vec4 yAxis = textureLod(ColorMap, uvYAxis, mipLevel);
		vec4 zAxis = textureLod(ColorMap, uvZAxis, mipLevel);

		Albedo = xAxis * blending.x + yAxis * blending.y + zAxis * blending.z;

		vec2 specXAxis = textureLod(MetallicRoughnessMap, uvXAxis, mipLevel).rg;
		vec2 specYAxis = textureLod(MetallicRoughnessMap, uvYAxis, mipLevel).rg;
		vec2 specZAxis = textureLod(MetallicRoughnessMap, uvZAxis, mipLevel).rg;

		MetallicRoughnessSample = specXAxis * blending.x + specYAxis * blending.y + specZAxis * blending.z;

		if (HasNormalMap)
		{
			vec3 normXAxis = textureLod(NormalMap, uvXAxis, mipLevel).rgb;
			vec3 normYAxis = textureLod(NormalMap, uvYAxis, mipLevel).rgb;
			vec3 normZAxis = textureLod(NormalMap, uvZAxis, mipLevel).rgb;

			NormalSample = normXAxis * blending.x + normYAxis * blending.y + normZAxis * blending.z;
			//vertexNormal += (normSample - vec3(0.f, 0.f, 1.f)) * 2.f - 1.f;
		}

		if (HasEmissionMap)
		{
			vec3 emissionXAxis = textureLod(EmissionMap, uvXAxis, mipLevel).rgb;
			vec3 emissionYAxis = textureLod(EmissionMap, uvYAxis, mipLevel).rgb;
			vec3 emissionZAxis = textureLod(EmissionMap, uvZAxis, mipLevel).rgb;

			EmissionSample = emissionXAxis * blending.x + emissionYAxis * blending.y + emissionZAxis * blending.z;
		}
	}
	
	vec3 Normal = normalize(NormalMatrix * vertexNormal);
	//vec3 Normal = NormalSample * 2.f - 1.f;
	//Normal = normalize(Frag_TBN * Normal);
	//Normal = normalize(Normal + (NormalSample * 2.f - 1.f));
	
	Albedo.w -= Frag_Transparency;
	
	Albedo = vec4(Albedo.xyz * Frag_Paint.xyz, Albedo.w * Frag_Paint.w);

	if (Albedo.a < AlphaCutoff)
		discard;
	
	if (DebugOverdraw)
	{
		// accumulate a red color with overdraw
		// 27/10/2024 i rlly thought this would work but nah not really
		float prevValue = texture(FrameBuffer, gl_FragCoord.xy).r;
		if (prevValue > 0.f)
			FragColor = vec4(1.f, 0.f, 0.f, 1.f);
		//gl_FragDepth = 0.f;
		
		//FragColor = vec4(prevValue, 0.f, 0.f, 1.f);
		
		return;
	}

	vec3 LightInfluence = vec3(0.f, 0.f, 0.f);

	vec3 reflectDir = reflect(-ViewDirection, Normal);
	//reflectDir.y = -reflectDir.y;
	//vec3 ReflectedTint = textureLod(SkyboxCubemap, reflectDir, MetallicRoughnessSample.x * MetallnessFactor * 6.f).xyz;
	//ReflectedTint *= mix(vec3(1.f, 1.f, 1.f), Frag_ColorTint, MetallicRoughnessSample.x * MetallnessFactor);

	//Albedo = vec4(Albedo.xyz * Frag_ColorTint + ReflectedTint * MetallicRoughnessSample.y, Albedo.w);

	//Albedo = vec4(mix(ReflectedTint, Albedo.xyz * Frag_ColorTint, MetallicRoughnessSample.y * RoughnessFactor), Albedo.w);
	
	if (EmissionStrength <= 0)
		for (int LightIndex = 0; LightIndex < NumLights; LightIndex++)
			LightInfluence += CalculateLight(
				LightIndex,
				Normal,
				ViewDirection,
				MetallicRoughnessSample.y
			);
	else
		LightInfluence = EmissionSample * EmissionStrength + LightAmbient;
	
	LightInfluence += LightAmbient;
	vec3 FragCol3 = (LightInfluence/* + textureLod(SkyboxCubemap, reflectDir, 11).xyz*/);

	if (!DebugLightInfluence)
		FragCol3 *= Albedo.xyz;

	if (Fog)
	{
		//float Depth = LogisticDepth(0.01f, 100.0f);

		float FogStart = 200;
		float FogEnd = 50000;

		float FogDensity = FogStart / FogEnd;

		float Distance = LinearizeDepth(gl_FragCoord.z) * FarZ;
		float FogFactor = clamp((FogEnd - Distance) / (FogEnd - FogStart), 0.0, 1.0); //Linear fog

		//float FogFactor = pow(2.0f, -pow(Distance * FogDensity, 2));
		
		FragCol3 = FogColor + (FragCol3 - FogColor) * FogFactor;
	}

	FragColor = vec4(FragCol3, Albedo.w);
}
