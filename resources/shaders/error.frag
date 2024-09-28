// error.frag - Error shader used when a Shader or Shader Program cannot be loaded

#version 460 core

// ALL of the worldUber.frag uniforms need to be here to not get
// "location is invalid" crashes.

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

// the actual number of textures the mesh has/lights in the scene
uniform int NumDiffuseTextures = 1;
uniform int NumSpecularTextures = 1;
uniform int NumLights = 0;

// uniform arrays
uniform sampler2D DiffuseTextures[MAX_DIFFUSE_TEXTURES];
uniform sampler2D SpecularTextures[MAX_SPECULAR_TEXTURES];
//uniform sampler2D ShadowMaps[MAX_SHADOWCASTING_LIGHTS]; <-- Replaced with shadow atlas!
uniform LightObject Lights[MAX_LIGHTS];


uniform vec3 LightAmbient = vec3(.05f,.05f,.05f);
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

out vec4 FragColor;

void main()
{
	// Bright magenta
	FragColor = vec4(1.0f, 0.0f, 1.0f, 1.0f);
}
