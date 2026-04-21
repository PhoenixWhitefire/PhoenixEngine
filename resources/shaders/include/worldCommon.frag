#version 460 core

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
