// Uber shader for world geometry

// TODO: Remove unnecessary variables
// - 21/06/2024

#version 460 core

// for static array size
const int MAX_DIFFUSE_TEXTURES = 6;
const int MAX_SPECULAR_TEXTURES = 6;
const int MAX_LIGHTS = 32;
const int MAX_LIGHTS_SHADOWCASTING = 16;

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
	
	// generic, applies to all light types (i.e., directional, point, spot lights)
	vec3 Position;
	vec3 Color;

	// point lights and spotlights
	float Range;
	
	// spotlights only, controls the interpolation between light and dark to prevent a hard cut-off
	float Spot_OuterCone;
	float Spot_InnerCone;

	// shadows
	// NOTE: not functional :( too lazy lol
	//bool HasShadowMap;
	//int ShadowMapIndex;
	//mat4 ShadowMapProjection;
	// 21/06/2024 When did I write this??? Some stuff for a shadow atlas system...
	bool HasShadow;
	vec2 ShadowTexelPosition;
	vec2 ShadowTexelSize;
	mat4 ShadowProjection;
};

// Cubemaps for reflections
struct Cubemap
{
	// position in the scene
	vec3 Position;
	
	// cubemap itself
	samplerCube Texture;

	// for projection
	float NearPlane;
	float FarPlane;
};

uniform sampler2D ShadowAtlas;

// the actual number of textures the mesh has
uniform int NumDiffuseTextures = 1;
uniform int NumSpecularTextures = 1;

uniform int NumLights = 0;

// uniform arrays
uniform sampler2D DiffuseTextures[MAX_DIFFUSE_TEXTURES];
uniform sampler2D SpecularTextures[MAX_SPECULAR_TEXTURES];
//uniform sampler2D ShadowMaps[MAX_SHADOWCASTING_LIGHTS]; <-- Replaced with shadow atlas!
uniform LightObject Lights[MAX_LIGHTS];

uniform vec3 LightAmbient = vec3(.2f,.2f,.2f);
uniform float SpecularMultiplier = 1.f;
uniform float SpecularPower = 32.0f;

uniform float Reflectivity = 0.f;
uniform float Transparency = 0.f;

uniform vec3 ColorTint = vec3(1.f, 1.f, 1.f);

// cubemap used for reflections
// 21/06/2024: Not quite yet, though!
uniform Cubemap ReflectionCubemap;

uniform samplerCube SkyboxCubemap;

uniform float NearZ = 0.1f;
uniform float FarZ = 1000.0f;

uniform int Fog = 0;
uniform vec3 FogColor = vec3(0.85f, 0.85f, 0.90f);

uniform vec3 CameraPosition;

in vec3 Frag_VertexColor;
in vec2 Frag_UV;

in vec3 Frag_VertexNormal;
in vec3 Frag_CurrentPosition;

in mat4 Frag_ModelMatrix;

in mat4 Frag_CamMatrix;

out vec4 FragColor;

float GetShadowValueForLight(int LightIndex, vec3 Normal)
{
	LightObject Light = Lights[LightIndex];
	
	if (!Light.HasShadow)
		return 0.0f;

	// prevents shadow acne
	float Bias = max(0.025f * (1.0f - dot(Normal, normalize(Lights[LightIndex].Position))), 0.0005f);

	float ShadowAmount = 0.0f;

	vec4 RelativeToLight = Light.ShadowProjection * vec4(Frag_CurrentPosition, 1.0f);

	vec3 LightCoords = RelativeToLight.xyz / RelativeToLight.z;

	// Blur more when the surface is further from the shadow caster
	const bool DistanceBlur = false;
	const int MaxSampleRadius = 8;

	if(LightCoords.z > 1.0f)
	{
		// Get from [-1, 1] range to [0, 1] range just like the shadow map
		LightCoords = (LightCoords + vec3(1.f)) / 2.0f;
		float CurrentDepth = LightCoords.z;

		// Smoothens out the shadows
		int SampleRadius = 1;

		vec2 TexelPosition = 1.0f / Light.ShadowTexelPosition;
		vec2 PixelSize = 1.0f / Light.ShadowTexelSize;

		if (DistanceBlur)
			SampleRadius = int(ceil(LightCoords.z * float(MaxSampleRadius)));

		for (int Y = -SampleRadius; Y <= SampleRadius; Y++)
		{
			for (int X = -SampleRadius; X <= SampleRadius; X++)
			{
				float ClosestDepth = texture(ShadowAtlas, LightCoords.xy * PixelSize + TexelPosition).r;

				if (CurrentDepth > ClosestDepth)
					ShadowAmount += 1.0f;   
			}   
		}

		// get average
		//Shadow /= pow((SampleRadius * 2 + 1), 2);
	}

	return ShadowAmount;
}

float PointLight(vec3 Direction, float Range)
{
	float Distance = length(Direction);

	/*
	float a = 3.0f;
	float b = 0.7f;

	float Intensity = 1.0f / (a * Distance * Distance + b * Distance + 1.0f);
	*/

	//return 1-clamp(Distance/Range, 0.0f, 1.0f);

	return 1.f / (pow(Distance, 2.f));
}

float SpotLight(vec3 Direction, float OuterCone, float InnerCone)
{
	float Angle = dot(vec3(0.0f, -1.0f, 0.0f), -normalize(Direction));

	return clamp(Angle + (OuterCone / InnerCone) * (Angle * OuterCone), 0.0f, 1.0f);
}

float LinearizeDepth()
{
	return (2.0f * NearZ * FarZ) / (FarZ + NearZ - (gl_FragCoord.z * 2.0f - 1.0f) * (FarZ - NearZ));
}

float LogisticDepth(float Steepness, float Offset)
{
	float ZVal = LinearizeDepth();

	return (1 / (1 + exp(-Steepness * (ZVal - Offset))));
}

//Calculates what a light would add to a pixel
vec3 CalculateLight(int Index, vec3 Normal, vec3 ViewDirection, vec3 Albedo, float SpecMapValue, float Shadow)
{
	LightObject Light = Lights[Index];

	vec3 LightPosition = Light.Position;
	vec3 LightColor = Light.Color;

	int LightType = Light.Type;

	vec3 FinalColor;

	if (LightType == 0)
	{
		vec3 LightDirection = normalize(LightPosition);
		float Diffuse = max(dot(Normal, LightDirection), 0.0f);

		float Specular = 0.f;

		if (Diffuse > 0.0f && Shadow == 0.0f) {
			vec3 reflectDir = reflect(-LightDirection, Normal);
			float specAmt = pow(max(dot(ViewDirection, reflectDir), 0.f), 1.f);
			Specular = specAmt * 0.5f;
		}

		FinalColor = Albedo * (Diffuse + (SpecMapValue * Specular)) * LightColor * (1.0f - Shadow);
		//FinalColor = Albedo * SpecMapValue;
	}
	else if (LightType == 1)
	{
		vec3 LightToPosition = LightPosition - Frag_CurrentPosition;
		vec3 LightDirection = normalize(LightToPosition);

		float Diffuse = max(dot(Normal, LightDirection), 0.0f);

		float Specular = 0.0f;

		if (Diffuse > 0.0f && Shadow == 0.0f)
		{
			vec3 Halfway = normalize(ViewDirection + LightDirection);

			Specular = pow(max(dot(Normal, Halfway), 0.0f), SpecularPower) * SpecularMultiplier;
		}

		float Intensity = PointLight(LightToPosition, Light.Range);
		float SpecularTerm = (SpecMapValue * Specular * Intensity);

		FinalColor = Albedo * ((Diffuse * Intensity) + SpecularTerm) * LightColor * (1.0f - Shadow);
	}
	else if (LightType == 2.0f)
	{
		vec3 LightToPosition = LightPosition - Frag_CurrentPosition;
		vec3 LightDirection = normalize(LightToPosition);

		float Diffuse = max(dot(Normal, LightDirection), 0.0f);

		float Specular = 0.0f;

		if (Diffuse > 0.0f && Shadow == 0.0f)
		{
			vec3 Halfway = normalize(ViewDirection + LightDirection);

			Specular = pow(max(dot(Normal, Halfway), 0.0f), SpecularPower) * SpecularMultiplier;
		}

		float Intensity = SpotLight(LightToPosition, Light.Spot_InnerCone, Light.Spot_OuterCone);
		float SpecularTerm = (SpecMapValue * Specular * Intensity);

		FinalColor = Albedo * ((Diffuse * Intensity) + SpecularTerm) * LightColor * (1.0f - Shadow);
	}
	else
	{
		FinalColor = vec3(1.0f, 0.0f, 1.0f);
	}

	return FinalColor;
}

void main()
{
	/*
	float l = length(texture(ShadowMaps[0], gl_FragCoord.xy));
	
	FragColor = vec4(l, l, l, 1.0f);

	return;
	*/

	// Convert mesh normals to world-space
	mat3 NormalMatrix = transpose(inverse(mat3(Frag_ModelMatrix)));
	vec3 Normal = normalize(NormalMatrix * Frag_VertexNormal);

	vec2 UV = Frag_UV;

	vec3 ViewDirection = CameraPosition - Frag_CurrentPosition;

	float SpecMapValue = 1.0f;
	vec4 Albedo = vec4(0.0f, 0.0f, 0.0f, 1.0f);

	for (int TexIdx = 0; TexIdx <= NumDiffuseTextures; TexIdx++)
	{
	    float mipmapLevel = textureQueryLod(DiffuseTextures[TexIdx], UV).x;
        vec4 texColMipped = textureLod(DiffuseTextures[TexIdx], UV, mipmapLevel);
        Albedo += vec4(texColMipped.xyz * texColMipped.w, 0.0f);

		if (texColMipped.w < Albedo.w)
		{
			Albedo = vec4(Albedo.xyz, texColMipped.w);
		}
	}

	FragColor = Albedo;

	if (NumSpecularTextures > 0)
	{
		SpecMapValue = 0.0f;
		
		for (int TexIdx = 0; TexIdx < NumSpecularTextures; TexIdx++)
		{
			SpecMapValue += texture(SpecularTextures[TexIdx], UV).x;
		}
	}

	Albedo -= vec4(0.f, 0.f, 0.f, Transparency);

	// completely transparent region
	// alpha is for some reason never 0?? <--- Specifically on the dev.world grass mesh
	if (Albedo.a < 0.2f)
		discard;
	
	vec3 FinalColor = vec3(0.f, 0.f, 0.f);

	float FresnelFactor = 0.0f;//1.f - clamp(0.0f + 1.f * pow(1.0f + dot(vec3(Frag_CamMatrix[3]), ViewDirection), 1.f), 0.f, 1.f);
	float ReflectivityFactor = Reflectivity + FresnelFactor;

	vec3 ReflectedTint = texture(SkyboxCubemap, Normal).xyz;
	const vec3 WHITE = vec3(1.f, 1.f, 1.f);

	vec3 Albedo3 = Albedo.xyz * Frag_VertexColor * ColorTint * mix(WHITE, ReflectedTint, ReflectivityFactor);
	float AccumulatedShadow = 0.0f;

	/*
	for (int LightIdx = 0; LightIdx < NumLights; LightIdx++)
		AccumulatedShadow += GetShadowValueForLight(LightIdx, Normal);
	
	AccumulatedShadow /= NumLights;

	FragColor = vec4(texture(ShadowMaps[0], gl_FragCoord.xy).x * 100.f);

	return;
	*/

	for (int LightIndex = 0; LightIndex < NumLights; LightIndex++)
	{
		FinalColor = FinalColor + CalculateLight(
			LightIndex,
			Normal,
			ViewDirection,
			Albedo3,
			SpecMapValue,
			AccumulatedShadow
		);
	}

	FinalColor += LightAmbient * Albedo3;

	vec3 FragCol3 = FinalColor.xyz;

	/*
	if (Fog == 1)
	{
		//float Depth = LogisticDepth(0.01f, 100.0f);

		float FogStart = 200;
		float FogEnd = 5000;

		float FogDensity = FogStart / FogEnd;

		float Distance = LinearizeDepth() * FarZ;
		float FogFactor = clamp((FogEnd - Distance) / (FogEnd - FogStart), 0.0, 1.0); //Linear fog

		//float FogFactor = pow(2.0f, -pow(Distance * FogDensity, 2));
		
		FragCol3 = FogColor + (FragCol3 - FogColor) * FogFactor;
	}
	*/

	FragColor = vec4(FragCol3, Albedo.w);
}
