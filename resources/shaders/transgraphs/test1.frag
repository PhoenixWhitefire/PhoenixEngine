// Initial state of Shader Graph -> GLSL transpiler output

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

uniform int NumLights = 0;

uniform sampler2D FrameBuffer;

uniform sampler2D ColorMap;
uniform sampler2D MetallicRoughnessMap;
uniform bool HasNormalMap;
uniform sampler2D NormalMap;
uniform bool HasEmissionMap;
uniform sampler2D EmissionMap;

uniform vec3 LightAmbient = vec3(0.3f);
uniform float SpecularMultiplier = 0.5f;
uniform float SpecularPower = 16.0f;

uniform float Transparency = 0.f;
uniform float MetallnessFactor = 0.f;
uniform float RoughnessFactor = 0.f;
uniform float EmissionStrength = 0.f;

uniform samplerCube SkyboxCubemap;

uniform float NearZ = 0.1f;
uniform float FarZ = 1000.0f;

uniform bool Fog = false;
uniform vec3 FogColor = vec3(0.85f, 0.85f, 0.90f);

uniform bool UseTriPlanarProjection = false;
uniform float MaterialProjectionFactor = 0.05f;

uniform float AlphaCutoff = 0.5f;

uniform bool IsShadowMap = false;
uniform bool DebugOverdraw = false;
uniform bool DebugLightInfluence = false;

in vec3 Frag_ModelPosition;
in vec3 Frag_WorldPosition;
in vec3 Frag_VertexNormal;
in vec3 Frag_ColorTint;
in vec2 Frag_TextureUV;
in mat4 Frag_Transform;
in vec4 Frag_RelativeToDirecLight;
//in mat3 Frag_TBN;
in vec3 Frag_CameraPosition;

out vec4 FinalOutput;

void main()
{
	
// CONST VARS



// NODES

FinalOutput = vec4(Frag_ColorTint, 1.f);


//END SHADER GRAPH TRANSPILATION

}