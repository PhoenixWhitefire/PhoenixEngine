// EmbeddedFiles.hpp, 09/10/2026
#include <unordered_map>
#include <string_view>

static const std::unordered_map<std::string_view, std::string_view> EmbeddedFiles = {
	{ "phoenix://materials/error.mtl", R"(
{
  "ColorMap": "phoenix://textures/Missing",
  "HasTranslucency": false,
  "MetallicRoughnessMap": "phoenix://textures/Black",
  "Shader": "worldUber",
  "Uniforms": {
    "EmissionStrength": 1.0
  },
  "BilinearFiltering": false,
  "specExponent": 1.0,
  "specMultiply": 0.5
}
	)" },

    { "phoenix://materials/Checkered.mtl", R"(
{
  "ColorMap": "phoenix://textures/Checkered",
  "HasTranslucency": false,
  "MetallicRoughnessMap": "phoenix://textures/White",
  "PolygonMode": 0,
  "Shader": "phoenix://shaders/worldUberTriProjected.shp",
  "Uniforms": {
    "MaterialProjectionFactor": 0.05000000074505806
  },
  "BilinearFiltering": false,
  "specExponent": 4.0,
  "specMultiply": 0.25
}
    )" },

    { "phoenix://materials/Smooth.mtl", R"(
{
  "ColorMap": "phoenix://textures/White",
  "HasTranslucency": false,
  "MetallicRoughnessMap": "phoenix://textures/White",
  "PolygonMode": 0,
  "Shader": "phoenix://shaders/worldUber.shp",
  "specExponent": 4.0,
  "specMultiply": 0.25
}
    )" },

    { "phoenix://shaders/error.shp", R"(
{
    "Vertex": "phoenix://shaders/worldUber.vert",
    "Fragment": "phoenix://shaders/error.frag"
    //"InheritUniformsOf": "worldUber"
}
    )" },

    { "phoenix://shaders/error.frag", R"(
// Uber shader for world geometry

// TODO: Remove unnecessary variables
// - 21/06/2024

#version 460 core
#extension GL_ARB_shading_language_include : require

#include "/include/worldCommon.frag"

in vec3 Frag_ModelPosition;
in vec3 Frag_WorldPosition;
in vec3 Frag_VertexNormal;
in vec3 Frag_ColorTint;
in vec2 Frag_TextureUV;
in mat4 Frag_Transform;
in vec4 Frag_RelativeToDirecLight;
//in mat3 Frag_TBN;
in vec3 Frag_CameraPosition;

out vec4 FragColor;

void main()
{
	// Bright magenta
	FragColor = vec4(1.f, 0.f, 1.f, 1.f);
}
    )" },

    { "phoenix://shaders/ui.shp", R"(
{
  "Fragment": "phoenix://shaders/ui.frag",
  "Vertex": "phoenix://shaders/ui.vert"
}
    )" },

    { "phoenix://shaders/ui.vert", R"(
// User Interface vertex shader, 12/03/2026
#version 460 core

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec4 VertexPaint;
layout (location = 3) in vec2 VertexUV;

uniform vec2 Phoenix_Position;
uniform vec2 Phoenix_Size;

out vec2 Frag_UV;

void main()
{
    gl_Position = vec4(vec3((VertexPosition.xy * 2.f) * Phoenix_Size + Phoenix_Position, -1.f), 1.f);
    Frag_UV = VertexUV;
}
    )" },

    { "phoenix://shaders/ui.frag", R"(
// User Interface fragment shader, 12/03/2026
#version 460 core

uniform vec3 Phoenix_BackgroundColor;
uniform float Phoenix_BackgroundTransparency;
uniform bool Phoenix_IsImage = false;
uniform sampler2D Phoenix_Image;

in vec2 Frag_UV;
out vec4 FragColor;

void main()
{
    if (!Phoenix_IsImage)
    {
        FragColor = vec4(Phoenix_BackgroundColor, 1.f - Phoenix_BackgroundTransparency);
    }
    else
    {
        vec4 imageCol = texture(Phoenix_Image, Frag_UV);
        FragColor = vec4(vec3(imageCol) * Phoenix_BackgroundColor, imageCol.a - Phoenix_BackgroundTransparency);
    }

    FragColor = vec4(pow(FragColor.xyz, vec3(1.f / 2.2f)), FragColor.a);
}
    )" },

    { "phoenix://shaders/worldUber.shp", R"(
{
  "Fragment": "phoenix://shaders/worldUber.frag",
  "Geometry": "phoenix://shaders/worldUber.geom",
  "InheritUniformsOf": "",
  "Uniforms": {
    "Phoenix_Material.AlphaCutoff": 0.05000000074505806,
    "Phoenix_Material.EmissionStrength": 0.0,
    "Phoenix_Material.SpecularMultiplier": 0.5,
    "Phoenix_Material.SpecularPower": 8.0
  },
  "Vertex": "phoenix://shaders/worldUber.vert"
}
    )" },

    { "phoenix://shaders/worldUber.vert", R"(
// Uber shader for world geometry
#version 460 core

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec4 VertexPaint;
layout (location = 3) in vec2 VertexUV;
// from Instanced Array
layout (location = 4) in mat4 InstanceTransform;
layout (location = 8) in vec3 InstanceColor;
layout (location = 9) in float InstanceTransparency;

uniform mat4 Phoenix_RenderMatrix;

uniform mat4 Phoenix_Transform;
uniform vec3 Phoenix_ColorTint;
uniform bool Phoenix_IsInstanced;

uniform float Phoenix_Time;
uniform mat4 Phoenix_DirectionalLightProjection;

uniform vec3 Phoenix_CameraPosition;

out DATA
{
	vec3 VertexNormal;
	vec2 TextureUV;
	vec4 Paint;
	mat4 RenderMatrix;
	float Transparency;

	vec3 ModelPosition;
	vec3 WorldPosition;
	mat4 Transform;
	vec4 RelativeToDirecLight;
	vec3 CameraPosition;
} data_out;

vec3 getMatrixScale(mat4 m)
{
	return vec3(
		length(m[0]),
		length(m[1]),
		length(m[2])
	);
}

void main()
{
	mat4 trans = Phoenix_Transform;
	vec4 pain = vec4(Phoenix_ColorTint, 1.f) * VertexPaint;

	if (Phoenix_IsInstanced)
	{
		trans = InstanceTransform;
		pain = vec4(InstanceColor, 1.f) * VertexPaint;
	}

	vec4 worldPos = trans * vec4(VertexPosition, 1.0f);

	data_out.VertexNormal = VertexNormal;
	data_out.Paint = pain;
	data_out.TextureUV = VertexUV;
	data_out.Transparency = InstanceTransparency;
	data_out.RenderMatrix = Phoenix_RenderMatrix;
	data_out.ModelPosition = VertexPosition * getMatrixScale(trans);
	data_out.WorldPosition = vec3(worldPos);
	data_out.Transform = trans;
	data_out.RelativeToDirecLight = Phoenix_DirectionalLightProjection * worldPos;
	data_out.CameraPosition = Phoenix_CameraPosition;

	gl_Position = vec4(data_out.WorldPosition, 1.f);
}
    )" },

    { "phoenix://shaders/worldUber.frag", R"(
// Uber shader for world geometry

// TODO: Remove unnecessary variables
// - 21/06/2024

#version 460 core
#extension GL_ARB_shading_language_include : require

#include "/include/worldCommon.frag"

#ifndef USE_TRI_PLANAR_PROJECTION
#define USE_TRI_PLANAR_PROJECTION false
#endif

uniform float MaterialProjectionFactor;

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

float SpotLight(vec3 LightToPosition, vec3 Direction, float OuterCone, float InnerCone, float Range)
{
	float Angle = dot(Direction, -normalize(LightToPosition));

	return 1.f - clamp((Angle - OuterCone) / (InnerCone - OuterCone), 0.0f, 1.0f);
}

float LinearizeDepth(float d)
{
	return (2.0f * Phoenix_NearZ * Phoenix_FarZ) / (Phoenix_FarZ + Phoenix_NearZ - (d * 2.0f - 1.0f) * (Phoenix_FarZ - Phoenix_NearZ));
}

float LogisticDepth(float Steepness, float Offset)
{
	float ZVal = LinearizeDepth(gl_FragCoord.z);

	return (1 / (1 + exp(-Steepness * (ZVal - Offset))));
}

// Calculates what a light would add to a pixel
vec3 CalculateLight(int Index, vec3 Normal, vec3 Outgoing, float SpecMapValue)
{
	LightObject Light = Phoenix_Lights[Index];

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

				float bias = min(0.05 * (dot(Normal, Incoming)), 0.000005);

				const int SampleRadius = 1;
				vec2 texelSize = 1.f / textureSize(Phoenix_ShadowAtlas, 0);
				for (int y = -SampleRadius; y <= SampleRadius; y++)
					for (int x = -SampleRadius; x <= SampleRadius; x++)
					{
						float pcfDepth = texture(Phoenix_ShadowAtlas, lightCoords.xy + vec2(x, y) * texelSize).r;
						shadow += currentDepth - bias > pcfDepth ? 1.f : 0.f;
					}

				shadow /= pow((SampleRadius * 2 + 1), 2);
			}
		}

		float Intensity = max(dot(Normal, Incoming), 0.f);
		float Diffuse = Intensity;

		float Specular = 0.f;

		if (Diffuse > 0.f && shadow == 0.f)
		{
			vec3 reflectDir = reflect(-Incoming, Normal);
			Specular = pow(max(dot(Outgoing, reflectDir), 0.f), Phoenix_Material.SpecularPower);
		}

		float SpecularTerm = SpecMapValue * Specular * Phoenix_Material.SpecularMultiplier;

		return (Diffuse * (1.f - shadow) + SpecularTerm * (1.f - shadow) * Intensity) * LightColor;
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

			Specular = pow(max(dot(Outgoing, reflectionVector), 0.f), Phoenix_Material.SpecularPower);
		}

		float Intensity = PointLight(LightToPosition, Light.Range);
		float SpecularTerm = SpecMapValue * Specular * Phoenix_Material.SpecularMultiplier;

		return ((Diffuse * Intensity) + SpecularTerm * Intensity) * LightColor;
	}
	else
	{
		vec3 LightToPosition = LightPosition - Frag_WorldPosition;
		vec3 Incoming = normalize(LightToPosition);

		float Diffuse = max(dot(Normal, Incoming), 0.0f);

		float Specular = 0.0f;

		if (Diffuse > 0.0f)
		{
			//vec3 Halfway = normalize(ViewDirection + LightDirection);

			vec3 reflectionVector = reflect(-Incoming, Normal);

			Specular = pow(max(dot(Outgoing, reflectionVector), 0.f), Phoenix_Material.SpecularPower);
		}

		float Intensity = SpotLight(LightToPosition, Light.SpotLightDirection, Light.Angle, Light.Angle - 0.05f, Light.Range);
		float SpecularTerm = SpecMapValue * Specular * Phoenix_Material.SpecularMultiplier;

		return ((Diffuse * Intensity) + SpecularTerm * Intensity) * LightColor;
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
//#define DEBUG_DRAWSHADOWMAP
#ifdef DEBUG_DRAWSHADOWMAP

	if (gl_FragCoord.x < 512 && gl_FragCoord.y < 512)
	{
		FragColor = vec4(texture(Phoenix_ShadowAtlas, gl_FragCoord.xy / 512.f).rrr, 1.f);
		return;
	}

#endif

	float mipLevel = textureQueryLod(Phoenix_Material.ColorMap, Frag_TextureUV).x;

	if (Phoenix_IsShadowMap)
	{
		if (textureLod(Phoenix_Material.ColorMap, Frag_TextureUV, mipLevel).w < Phoenix_Material.AlphaCutoff)
			discard;

		FragColor = vec4(gl_FragCoord.z, gl_FragCoord.z, gl_FragCoord.z, 1.f);

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

	if (!USE_TRI_PLANAR_PROJECTION)
	{
		MetallicRoughnessSample = textureLod(Phoenix_Material.MetallicRoughnessMap, Frag_TextureUV, mipLevel).rg;
		Albedo = textureLod(Phoenix_Material.ColorMap, Frag_TextureUV, mipLevel);

		if (Phoenix_Material.HasNormalMap)
			NormalSample = (textureLod(Phoenix_Material.NormalMap, Frag_TextureUV, mipLevel).xyz - vec3(0.f, 0.f, 1.f)) * 2.f - 1.f;

		if (Phoenix_Material.HasEmissionMap)
			EmissionSample = textureLod(Phoenix_Material.EmissionMap, Frag_TextureUV, mipLevel).xyz;
	}
	else
	{
		vec3 blending = getTriPlanarBlending(Frag_VertexNormal);
		//vec2 uvXAxis = Frag_ModelPosition.zy * vec2(1.f, -1.f) * MaterialProjectionFactor;
		//vec2 uvYAxis = Frag_ModelPosition.xz * vec2(-1.f, 1.f) * MaterialProjectionFactor;
		//vec2 uvZAxis = -Frag_ModelPosition.xy * MaterialProjectionFactor;

		vec4 xAxis = textureLod(Phoenix_Material.ColorMap, Frag_ModelPosition.yz, mipLevel);
		vec4 yAxis = textureLod(Phoenix_Material.ColorMap, Frag_ModelPosition.xz, mipLevel);
		vec4 zAxis = textureLod(Phoenix_Material.ColorMap, Frag_ModelPosition.xy, mipLevel);

		Albedo = xAxis * blending.x + yAxis * blending.y + zAxis * blending.z;

		vec2 specXAxis = textureLod(Phoenix_Material.MetallicRoughnessMap, Frag_ModelPosition.yz, mipLevel).rg;
		vec2 specYAxis = textureLod(Phoenix_Material.MetallicRoughnessMap, Frag_ModelPosition.xz, mipLevel).rg;
		vec2 specZAxis = textureLod(Phoenix_Material.MetallicRoughnessMap, Frag_ModelPosition.xy, mipLevel).rg;

		MetallicRoughnessSample = specXAxis * blending.x + specYAxis * blending.y + specZAxis * blending.z;

		if (Phoenix_Material.HasNormalMap)
		{
			vec3 normXAxis = textureLod(Phoenix_Material.NormalMap, Frag_ModelPosition.yz, mipLevel).rgb;
			vec3 normYAxis = textureLod(Phoenix_Material.NormalMap, Frag_ModelPosition.xz, mipLevel).rgb;
			vec3 normZAxis = textureLod(Phoenix_Material.NormalMap, Frag_ModelPosition.xy, mipLevel).rgb;

			NormalSample = normXAxis * blending.x + normYAxis * blending.y + normZAxis * blending.z;
			//vertexNormal += (normSample - vec3(0.f, 0.f, 1.f)) * 2.f - 1.f;
		}

		if (Phoenix_Material.HasEmissionMap)
		{
			vec3 emissionXAxis = textureLod(Phoenix_Material.EmissionMap, Frag_ModelPosition.yz, mipLevel).rgb;
			vec3 emissionYAxis = textureLod(Phoenix_Material.EmissionMap, Frag_ModelPosition.xz, mipLevel).rgb;
			vec3 emissionZAxis = textureLod(Phoenix_Material.EmissionMap, Frag_ModelPosition.xy, mipLevel).rgb;

			EmissionSample = emissionXAxis * blending.x + emissionYAxis * blending.y + emissionZAxis * blending.z;
		}
	}

	vec3 Normal = normalize(NormalMatrix * vertexNormal);
	//vec3 Normal = NormalSample * 2.f - 1.f;
	//Normal = normalize(Frag_TBN * Normal);
	//Normal = normalize(Normal + (NormalSample * 2.f - 1.f));

	Albedo.w -= Frag_Transparency;

	Albedo = vec4(Albedo.xyz * Frag_Paint.xyz, Albedo.w * Frag_Paint.w);

	if (Albedo.a < Phoenix_Material.AlphaCutoff)
		discard;

	if (Phoenix_DebugOverdraw)
	{
		// accumulate a red color with overdraw
		// 27/10/2024 i rlly thought this would work but nah not really
		float prevValue = texture(Phoenix_FramebufferTexture, gl_FragCoord.xy).r;
		if (prevValue > 0.f)
			FragColor = vec4(1.f, 0.f, 0.f, 1.f);
		//gl_FragDepth = 0.f;

		//FragColor = vec4(prevValue, 0.f, 0.f, 1.f);

		return;
	}

	vec3 LightInfluence = vec3(0.f, 0.f, 0.f);

	vec3 reflectDir = reflect(-ViewDirection, Normal);
	//reflectDir.y = -reflectDir.y;
	//vec3 ReflectedTint = textureLod(SkyboxCubemap, reflectDir, MetallicRoughnessSample.x * MetalnessFactor * 6.f).xyz;
	//ReflectedTint *= mix(vec3(1.f, 1.f, 1.f), Frag_ColorTint, MetallicRoughnessSample.x * MetalnessFactor);

	//Albedo = vec4(Albedo.xyz * Frag_ColorTint + ReflectedTint * MetallicRoughnessSample.y, Albedo.w);

	//Albedo = vec4(mix(ReflectedTint, Albedo.xyz * Frag_ColorTint, MetallicRoughnessSample.y * RoughnessFactor), Albedo.w);

	if (Phoenix_Material.EmissionStrength <= 0)
		for (int LightIndex = 0; LightIndex < Phoenix_NumLights; LightIndex++)
			LightInfluence += CalculateLight(
				LightIndex,
				Normal,
				ViewDirection,
				MetallicRoughnessSample.y
			);
	else
		LightInfluence = EmissionSample * Phoenix_Material.EmissionStrength + Phoenix_LightAmbient;

	if (!Phoenix_DebugLightInfluence)
		LightInfluence += Phoenix_LightAmbient;
	vec3 FragCol3 = (LightInfluence/* + textureLod(SkyboxCubemap, reflectDir, 11).xyz*/);

	if (!Phoenix_DebugLightInfluence)
		FragCol3 *= Albedo.xyz;

	if (Phoenix_Fog)
	{
		//float Depth = LogisticDepth(0.01f, 100.0f);

		float FogStart = 200;
		float FogEnd = 50000;

		float FogDensity = FogStart / FogEnd;

		float Distance = LinearizeDepth(gl_FragCoord.z) * Phoenix_FarZ;
		float FogFactor = clamp((FogEnd - Distance) / (FogEnd - FogStart), 0.0, 1.0); //Linear fog

		//float FogFactor = pow(2.0f, -pow(Distance * FogDensity, 2));

		FragCol3 = Phoenix_FogColor + (FragCol3 - Phoenix_FogColor) * FogFactor;
	}

	FragColor = vec4(FragCol3, Albedo.w);
}
    )" },

    { "phoenix://shaders/worldUber.geom", R"(
#version 460

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

const int MAX_LIGHTS = 6;

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

out vec3 Frag_ModelPosition;
out vec3 Frag_WorldPosition;
out vec3 Frag_VertexNormal;
out vec4 Frag_Paint;
out vec2 Frag_TextureUV;
out mat4 Frag_Transform;
out vec4 Frag_RelativeToDirecLight;
//out mat3 Frag_TBN;
out vec3 Frag_CameraPosition;
out float Frag_Transparency;

in DATA
{
	vec3 VertexNormal;
	vec2 TextureUV;
	vec4 Paint;
	mat4 RenderMatrix;
	float Transparency;

	vec3 ModelPosition;
	vec3 WorldPosition;
	mat4 Transform;
	vec4 RelativeToDirecLight;
	vec3 CameraPosition;
} data_in[];

void main()
{
	/*
	vec3 edge0 = gl_in[1].gl_Position.xyz - gl_in[0].gl_Position.xyz;
	vec3 edge1 = gl_in[2].gl_Position.xyz - gl_in[0].gl_Position.xyz;
	vec2 deltaUV0 = data_in[1].TextureUV - data_in[0].TextureUV;
	vec2 deltaUV1 = data_in[2].TextureUV - data_in[0].TextureUV;

	float invDet = 1.f / (deltaUV0.x * deltaUV1.y - deltaUV1.x * deltaUV0.y);

	vec3 tangent = vec3(invDet * (deltaUV1.y * edge0 - deltaUV0.y * edge1));
	vec3 bitangent = vec3(invDet * (-deltaUV1.x * edge0 - deltaUV0.x * edge1));

	vec3 T = normalize(vec3(data_in[0].Transform * vec4(tangent, 0.f)));
	vec3 B = normalize(vec3(data_in[0].Transform * vec4(bitangent, 0.f)));
	vec3 N = data_in[0].VertexNormal; //normalize(vec3(data_in[0].Transform * vec4(cross(edge1, edge0), 0.f)));

	mat3 TBN = mat3(T, B, N);
	//TBN = transpose(TBN);
	*/

	gl_Position = data_in[0].RenderMatrix * gl_in[0].gl_Position;

	Frag_ModelPosition = data_in[0].ModelPosition;
	Frag_WorldPosition = data_in[0].WorldPosition;
	Frag_VertexNormal = data_in[0].VertexNormal;
	Frag_Paint = data_in[0].Paint;
	Frag_Transparency = data_in[0].Transparency;
	Frag_TextureUV = data_in[0].TextureUV;
	Frag_Transform = data_in[0].Transform;
	Frag_RelativeToDirecLight = data_in[0].RelativeToDirecLight;
	//Frag_TBN = TBN;
	Frag_CameraPosition = data_in[0].CameraPosition;

	EmitVertex();

	gl_Position = data_in[1].RenderMatrix * gl_in[1].gl_Position;

	Frag_ModelPosition = data_in[1].ModelPosition;
	Frag_WorldPosition = data_in[1].WorldPosition;
	Frag_VertexNormal = data_in[1].VertexNormal;
	Frag_Paint = data_in[1].Paint;
	Frag_Transparency = data_in[1].Transparency;
	Frag_TextureUV = data_in[1].TextureUV;
	Frag_Transform = data_in[1].Transform;
	Frag_RelativeToDirecLight = data_in[1].RelativeToDirecLight;
	//Frag_TBN = TBN;
	Frag_CameraPosition = data_in[1].CameraPosition;

	EmitVertex();

	gl_Position = data_in[2].RenderMatrix * gl_in[2].gl_Position;

	Frag_ModelPosition = data_in[2].ModelPosition;
	Frag_WorldPosition = data_in[2].WorldPosition;
	Frag_VertexNormal = data_in[2].VertexNormal;
	Frag_Paint = data_in[2].Paint;
	Frag_Transparency = data_in[2].Transparency;
	Frag_TextureUV = data_in[2].TextureUV;
	Frag_Transform = data_in[2].Transform;
	Frag_RelativeToDirecLight = data_in[2].RelativeToDirecLight;
	//Frag_TBN = TBN;
	Frag_CameraPosition = data_in[2].CameraPosition;

	EmitVertex();

	EndPrimitive();
}
    )" },

    { "phoenix://shaders/worldUberSkinned.shp", R"(
{
  "Fragment": "phoenix://shaders/worldUber.frag",
  "Geometry": "phoenix://shaders/worldUber.geom",
  "InheritUniformsOf": "phoenix://shaders/worldUber.shp",
  "Vertex": "phoenix://shaders/worldUberSkinned.vert"
}
    )" },

    { "phoenix://shaders/worldUberSkinned.vert", R"(
// Uber shader for skinned geometry

#version 460 core

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec4 VertexPaint;
layout (location = 3) in vec2 VertexUV;
// from Instanced Array
layout (location = 4) in mat4 InstanceTransform;
layout (location = 8) in vec3 InstanceColor;
layout (location = 9) in float InstanceTransparency;
// skinned meshes
layout (location = 10) in uvec4 JointsIndices;
layout (location = 11) in vec4 JointsWeights;

uniform mat4 Phoenix_RenderMatrix;

uniform mat4 Phoenix_Transform;
uniform vec3 Phoenix_ColorTint;
uniform bool Phoenix_IsInstanced;

uniform float Phoenix_Time;
uniform mat4 Phoenix_DirectionalLightProjection;

uniform vec3 Phoenix_CameraPosition;

uniform mat4 Phoenix_BoneMatrices[128];

out DATA
{
	vec3 VertexNormal;
	vec2 TextureUV;
	vec4 Paint;
	mat4 RenderMatrix;
	float Transparency;

	vec3 ModelPosition;
	vec3 WorldPosition;
	mat4 Transform;
	vec4 RelativeToDirecLight;
	vec3 CameraPosition;
} data_out;

vec3 getMatrixScale(mat4 m)
{
	return vec3(
		length(m[0]),
		length(m[1]),
		length(m[2])
	);
}

void main()
{
	mat4 trans = Phoenix_Transform;
	vec4 pain = vec4(Phoenix_ColorTint, 1.f) * VertexPaint;

	if (Phoenix_IsInstanced)
	{
		trans = InstanceTransform;
		pain = vec4(InstanceColor, 1.f) * VertexPaint;
	}

	mat4 skin = mat4(1.f);

	if (JointsWeights.x + JointsWeights.y + JointsWeights.z + JointsWeights.w > 0.0)
	{
	    skin = JointsWeights.x * Phoenix_BoneMatrices[JointsIndices.x]
				+ JointsWeights.y * Phoenix_BoneMatrices[JointsIndices.y]
				+ JointsWeights.z * Phoenix_BoneMatrices[JointsIndices.z]
				+ JointsWeights.w * Phoenix_BoneMatrices[JointsIndices.w];
	}

	data_out.VertexNormal = VertexNormal;
	data_out.Paint = pain;//vec4(JointsIndices.xyz == vec3(0.f) ? vec3(1.f) : vec3(0.f), 1.f);
	data_out.TextureUV = VertexUV;
	data_out.Transparency = InstanceTransparency;
	data_out.RenderMatrix = Phoenix_RenderMatrix;
	data_out.ModelPosition = vec3(skin * vec4(VertexPosition, 1.f));
	data_out.WorldPosition = vec3(trans * vec4(data_out.ModelPosition, 1.0f));
	data_out.Transform = trans;
	data_out.RelativeToDirecLight = Phoenix_DirectionalLightProjection * vec4(data_out.WorldPosition, 1.f);
	data_out.CameraPosition = Phoenix_CameraPosition;

	gl_Position = vec4(data_out.WorldPosition, 1.f);
}
    )" },

    { "phoenix://shaders/worldUberTriProjected.shp", R"(
{
  "Definitions": {
    "USE_TRI_PLANAR_PROJECTION": "true"
  },
  "Fragment": "phoenix://shaders/worldUber.frag",
  "Geometry": "phoenix://shaders/worldUber.geom",
  "InheritUniformsOf": "phoenix://shaders/worldUber.shp",
  "Vertex": "phoenix://shaders/worldUber.vert"
}
    )" },

    { "phoenix://shaders/skybox.shp", R"(
{
  "Fragment": "phoenix://shaders/skybox.frag",
  "Geometry": "<NOT_SPECIFIED>",
  "InheritUniformsOf": "",
  "Vertex": "phoenix://shaders/skybox.vert"
}
    )" },

    { "phoenix://shaders/skybox.vert", R"(
// Skybox shader

#version 460 core

layout (location = 0) in vec3 VertexPosition;

out vec3 FragIn_Direction;

uniform mat4 Phoenix_RenderMatrix;

void main()
{
	vec4 Position = Phoenix_RenderMatrix * vec4(VertexPosition, 1.0f);

	gl_Position = Position.xyww;
	FragIn_Direction = VertexPosition.xyz;
}
    )" },

    { "phoenix://shaders/skybox.frag", R"(
// Skybox shader

#version 460 core

in vec3 FragIn_Direction;

out vec4 FragColor;

uniform samplerCube Phoenix_SkyboxCubemap;
uniform sampler2D Phoenix_SkyboxEquirectangular;
uniform bool Phoenix_IsSkyboxEquirectangular = false;
uniform bool Phoenix_HdrEnabled = false;
uniform bool Phoenix_DebugOverdraw = false;

uniform float Phoenix_Time = 0.f;

#define PI radians(180.f)

// https://discussions.unity.com/t/equirectangular-projection-shader-code/347527/3
vec2 RadialCoords(vec3 a_coords)
{
    vec3 a_coords_n = normalize(a_coords);
    float lon = atan(a_coords_n.z, a_coords_n.x);
    float lat = acos(a_coords_n.y);
    vec2 sphereCoords = vec2(lon, lat) * (1.0 / PI);
    return vec2(sphereCoords.x * 0.5 + 0.5, sphereCoords.y);
}

void main()
{
	if (Phoenix_DebugOverdraw)
	{
		FragColor = vec4(0.f, 0.f, 0.f, 1.f);
		return;
	}

	//FragColor = vec4(FragIn_Direction, 1.f);

	if (Phoenix_IsSkyboxEquirectangular)
	{
		vec2 equiUV = RadialCoords(FragIn_Direction);
		FragColor = vec4(texture(Phoenix_SkyboxEquirectangular, equiUV).xyz, 1.f);
	}
	else
	{
		FragColor = texture(Phoenix_SkyboxCubemap, FragIn_Direction);
	}
}
    )" },

    { "phoenix://shaders/particle.shp", R"(
{
    "Vertex": "phoenix://shaders/particle.vert",
    "Fragment": "phoenix://shaders/particle.frag"
}
    )" },

    { "phoenix://shaders/particle.vert", R"(
// particle.vert - Particles
#version 460 core

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec4 VertexPaint;
layout (location = 3) in vec2 VertexUV;

uniform mat4 Phoenix_RenderMatrix;
uniform vec3 Phoenix_Position;
uniform float Phoenix_Size;

out vec2 Frag_TextureUV;

void main()
{
	Frag_TextureUV = VertexUV;

	vec3 modelPosition = VertexPosition * vec3(Phoenix_Size);
	vec3 worldPosition = Phoenix_Position + modelPosition;

	gl_Position = Phoenix_RenderMatrix * vec4(worldPosition, 1.f);
}
    )" },

    { "phoenix://shaders/particle.frag", R"(
// Particle.frag - Particles

#version 460 core

uniform sampler2D Phoenix_Image;

uniform float Phoenix_Transparency;
uniform vec3 Phoenix_Tint = vec3(1.f, 1.f, 1.f);

in vec2 Frag_TextureUV;

out vec4 FragColor;

void main()
{
	vec4 color = texture(Phoenix_Image, Frag_TextureUV);
	color.a -= Phoenix_Transparency;

	if (color.a < 0.05f)
		discard;

	FragColor = vec4(color.rgb, color.a);
}
    )" },

    { "phoenix://shaders/postprocessing.shp", R"(
{
  "Fragment": "phoenix://shaders/postprocessing.frag",
  "Geometry": "<NOT_SPECIFIED>",
  "InheritUniformsOf": "",
  "Vertex": "phoenix://shaders/postprocessing.vert"
}
    )" },

    { "phoenix://shaders/postprocessing.vert", R"(
#version 460 core

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal; // unused
layout (location = 2) in vec4 VertexColor; // unused
layout (location = 3) in vec2 TexUV;

uniform vec3 Phoenix_Scale;

out vec2 Frag_UV;

void main()
{
	gl_Position = vec4(VertexPosition.x * 2.f, VertexPosition.y * 2.f, 0.f, 1.f);
	Frag_UV = vec2(TexUV.x, 1.f - TexUV.y);
}
    )" },

    { "phoenix://shaders/postprocessing.frag", R"(
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
    )" },

    { "phoenix://shaders/boxframe.shp", R"(
{
  "Fragment": "phoenix://shaders/boxframe.frag",
  "Geometry": "phoenix://shaders/worldUber.geom",
  "InheritUniformsOf": "",
  "Vertex": "phoenix://shaders/worldUber.vert"
}
    )" },

    { "phoenix://shaders/boxframe.frag", R"(
// Uber shader for world geometry

// TODO: Remove unnecessary variables
// - 21/06/2024

#version 460 core
#extension GL_ARB_shading_language_include : require

#include "/include/worldCommon.frag"

in vec3 Frag_ModelPosition;
in vec3 Frag_WorldPosition;
in vec3 Frag_VertexNormal;
in vec4 Frag_Paint;
in vec2 Frag_TextureUV;
in mat4 Frag_Transform;
in vec4 Frag_RelativeToDirecLight;
//in mat3 Frag_TBN;
in vec3 Frag_CameraPosition;

out vec4 FragColor;

void main()
{
	if (Frag_TextureUV.x < 0.02f || Frag_TextureUV.y < 0.02f || Frag_TextureUV.x > 0.98f || Frag_TextureUV.y > 0.98f)
		FragColor = texture(Phoenix_Material.ColorMap, Frag_TextureUV) * Frag_Paint;
	else
		discard;
}
    )" },

    { "phoenix://shaders/bloomextract.shp", R"(
{
  "Fragment": "phoenix://shaders/bloomextract.frag",
  "Geometry": "<NOT_SPECIFIED>",
  "InheritUniformsOf": "",
  "Vertex": "phoenix://shaders/postprocessing.vert"
}
    )" },

    { "phoenix://shaders/bloomextract.frag", R"(
// bloomextract.frag, 05/10/2026
#version 460 core

in vec2 Frag_UV;
out vec4 FragColor;

uniform sampler2D Phoenix_PostProcessBuffer;
uniform float Phoenix_BloomThreshold;

void main()
{
	vec4 col = texture(Phoenix_PostProcessBuffer, Frag_UV);

    if (col.r * 0.2126f + col.g * 0.7152f + col.b * 0.0722f >= Phoenix_BloomThreshold)
    {
        FragColor = col;
    }
    else
    {
        FragColor = vec4(0.f);
    }
}
    )" },

    { "phoenix://shaders/bloomseparated.shp", R"(
{
  "Fragment": "phoenix://shaders/bloomseparated.frag",
  "Geometry": "<NOT_SPECIFIED>",
  "InheritUniformsOf": "",
  "Vertex": "phoenix://shaders/postprocessing.vert"
}
    )" },

    { "phoenix://shaders/bloomseparated.frag", R"(
// bloomseparated.frag, 05/10/2026

#version 460 core

in vec2 Frag_UV;
out vec4 FragColor;

uniform sampler2D Phoenix_BloomSource;
uniform float Phoenix_BloomScale;
uniform float Phoenix_BloomStdDeviation;
uniform bool Phoenix_BlurHorizontal;

// https://github.com/GarrettGunnell/AcerolaFX-Godot/blob/main/Effects/Blur/acerolafx_blur.gd
#define PI (3.14159265359f)

void main()
{
    ivec2 screenSize = textureSize(Phoenix_BloomSource, 0);
    float minAxis = min(float(screenSize.x), float(screenSize.y));
    float pixelScale = 1.f / minAxis;
    int kernelSize = int(minAxis * Phoenix_BloomScale * 0.01f);

    float twoSigmaSquared = 2.f * pow(Phoenix_BloomStdDeviation, 2.f);
    float gaussianDenominator = sqrt(PI * twoSigmaSquared);

    float totalWeight = 1.f / gaussianDenominator;
    vec3 sum = vec3(0.f);

    if (Phoenix_BlurHorizontal)
    {
        for (int x = -kernelSize; x <= kernelSize; x++)
        {
            if (x == 0)
                continue;

            float distSq = float(pow(x, 2));
            float gaussNumerator = exp(-distSq / twoSigmaSquared);
            float gaussian = gaussNumerator / gaussianDenominator;

            sum += texture(Phoenix_BloomSource, Frag_UV + vec2(x * pixelScale, 0.f)).xyz * gaussian;
            totalWeight += gaussian;
        }
    }
    else
    {
        for (int y = -kernelSize; y <= kernelSize; y++)
        {
            if (y == 0)
                continue;

            float distSq = float(pow(y, 2));
            float gaussNumerator = exp(-distSq / twoSigmaSquared);
            float gaussian = gaussNumerator / gaussianDenominator;

            sum += texture(Phoenix_BloomSource, Frag_UV + vec2(0.f, y * pixelScale)).xyz * gaussian;
            totalWeight += gaussian;
        }
    }

    FragColor = vec4(sum / vec3(totalWeight), 1.f);
}
    )" },

    { "phoenix://shaders/include/worldCommon.frag", R"(
//#version 460 core

#ifndef USE_TRI_PLANAR_PROJECTION
#define USE_TRI_PLANAR_PROJECTION false
#endif

const int MAX_LIGHTS = 16;

/*
LIGHT TYPE IDS:
- Directional lights = 0
- Point lights = 1
- Spot lights = 2

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
	vec3 SpotLightDirection;
	float Angle;
};

uniform LightObject Phoenix_Lights[MAX_LIGHTS];

uniform sampler2D Phoenix_ShadowAtlas;

uniform int Phoenix_NumLights;

uniform sampler2D Phoenix_FramebufferTexture;

uniform struct
{
	sampler2D ColorMap;
	sampler2D MetallicRoughnessMap;
	sampler2D NormalMap;
	sampler2D EmissionMap;
	float SpecularMultiplier;
	float SpecularPower;
	float MetalnessFactor;
	float RoughnessFactor;
	float EmissionStrength;
	float AlphaCutoff;
	bool HasNormalMap;
	bool HasEmissionMap;
} Phoenix_Material;

uniform vec3 Phoenix_LightAmbient = vec3(0.3f);

uniform samplerCube Phoenix_SkyboxCubemap;

uniform float Phoenix_NearZ = 0.1f;
uniform float Phoenix_FarZ = 1000.0f;

uniform bool Phoenix_Fog;
uniform vec3 Phoenix_FogColor = vec3(0.85f, 0.85f, 0.90f);

uniform bool Phoenix_IsShadowMap;
uniform bool Phoenix_DebugOverdraw = false;
uniform bool Phoenix_DebugLightInfluence = false;
    )" }
};
