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

uniform float Transparency;
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

out vec4 FragColor;

void main()
{
	if (Frag_TextureUV.x < 0.02f || Frag_TextureUV.y < 0.02f || Frag_TextureUV.x > 0.98f || Frag_TextureUV.y > 0.98f)
		FragColor = texture(ColorMap, Frag_TextureUV) * Frag_Paint;
	else
		discard;
}
