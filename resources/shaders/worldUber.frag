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

uniform sampler2D ShadowAtlas;

uniform int NumLights = 0;

//uniform sampler2D ShadowMaps[MAX_SHADOWCASTING_LIGHTS]; <-- Replaced with shadow atlas!
uniform LightObject Lights[MAX_LIGHTS];

uniform sampler2D ColorMap;
uniform sampler2D MetallicRoughnessMap;

uniform vec3 LightAmbient = vec3(.2f,.2f,.2f);
uniform float SpecularMultiplier = 0.5f;
uniform float SpecularPower = 16.0f;

uniform float Reflectivity = 0.f;
uniform float Transparency = 0.f;

uniform vec3 ColorTint = vec3(1.f, 1.f, 1.f);

uniform samplerCube SkyboxCubemap;

uniform float NearZ = 0.1f;
uniform float FarZ = 1000.0f;

uniform bool Fog = false;
uniform vec3 FogColor = vec3(0.85f, 0.85f, 0.90f);

in vec3 Frag_VertexColor;
in vec2 Frag_UV;

in vec3 Frag_VertexNormal;
in vec3 Frag_CurrentPosition;

in mat4 Frag_Transform;

in mat4 Frag_CamMatrix;

out vec4 FragColor;

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
vec3 CalculateLight(int Index, vec3 Normal, vec3 Outgoing, float SpecMapValue)
{
	LightObject Light = Lights[Index];

	vec3 LightPosition = Light.Position;
	vec3 LightColor = Light.Color;

	int LightType = Light.Type;

	vec3 FinalColor;

	if (LightType == 0)
	{
		vec3 Incoming = normalize(LightPosition);
		float Diffuse = max(dot(Normal, Incoming), 0.0f);

		float Specular = 0.f;

		if (Diffuse > 0.0f)
		{
			vec3 reflectDir = reflect(-Incoming, Normal);
			float specAmt = pow(max(dot(Outgoing, reflectDir), 0.f), SpecularPower) * SpecularMultiplier;
			//Specular = specAmt * SpecularMultiplier;
		}

		FinalColor = (Diffuse + (SpecMapValue * Specular)) * LightColor;
		//FinalColor = Albedo * SpecMapValue;
	}
	else if (LightType == 1)
	{
		vec3 LightToPosition = LightPosition - Frag_CurrentPosition;
		vec3 Incoming = normalize(LightToPosition);

		float Diffuse = max(dot(Normal, Incoming), 0.0f);

		float Specular = 0.0f;

		if (Diffuse > 0.0f)
		{
			//vec3 Halfway = normalize(ViewDirection + LightDirection);

			vec3 reflectionVector = reflect(-Incoming, Normal);

			//Specular = pow(max(dot(Outgoing, reflectionVector), 0.f), SpecularPower);
		}

		float Intensity = PointLight(LightToPosition, Light.Range);
		float SpecularTerm = (SpecMapValue * Specular);

		FinalColor = ((Diffuse * Intensity) + SpecularTerm * Intensity) * LightColor;
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
	mat3 NormalMatrix = transpose(inverse(mat3(Frag_Transform)));
	vec3 Normal = normalize(NormalMatrix * Frag_VertexNormal);

	vec2 UV = Frag_UV;

	vec3 ViewDirection = vec3(Frag_CamMatrix) - Frag_CurrentPosition;

	float mipLevel = textureQueryLod(ColorMap, UV).x;

	vec4 Albedo = textureLod(ColorMap, UV, mipLevel);
	float SpecMapValue = textureLod(MetallicRoughnessMap, UV, mipLevel).r;

	FragColor = Albedo;

	Albedo -= vec4(0.f, 0.f, 0.f, Transparency);

	// completely transparent region
	// alpha is for some reason never 0?? <--- Specifically on the dev.world grass mesh
	if (Albedo.a < 0.1f)
		discard;
	
	vec3 LightInfluence = vec3(0.f, 0.f, 0.f);

	float FresnelFactor = 0.0f;//1.f - clamp(0.0f + 1.f * pow(1.0f + dot(vec3(Frag_CamMatrix[3]), ViewDirection), 1.f), 0.f, 1.f);
	float ReflectivityFactor = Reflectivity + FresnelFactor;

	vec3 ReflectedTint = texture(SkyboxCubemap, reflect(normalize(Frag_CurrentPosition - vec3(Frag_CamMatrix)), Normal)).xyz;

	vec3 Albedo3 = mix(Albedo.xyz * Frag_VertexColor * ColorTint, ReflectedTint, ReflectivityFactor);

	for (int LightIndex = 0; LightIndex < NumLights; LightIndex++)
	{
		LightInfluence = LightInfluence + CalculateLight(
			LightIndex,
			Normal,
			ViewDirection,
			SpecMapValue
		);
	}

	vec3 FragCol3 = LightInfluence * Albedo3;
	
	if (Fog)
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

	FragColor = vec4(FragCol3, Albedo.w);
}
