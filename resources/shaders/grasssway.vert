// Uber shader for world geometry

#version 460 core

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec4 VertexPaint;
layout (location = 3) in vec2 VertexUV;
// from Instanced Array
layout (location = 4) in mat4 InstanceTransform;
layout (location = 8) in vec3 InstanceScale;
layout (location = 9) in vec3 InstanceColor;
layout (location = 10) in float InstanceTransparency;

const int MAX_LIGHTS = 6;

uniform mat4 RenderMatrix;

uniform mat4 Transform;
uniform vec3 Scale;
uniform vec3 ColorTint;
uniform bool IsInstanced;

uniform float Time;
uniform mat4 DirecLightProjection;

uniform vec3 CameraPosition;

const vec3 WindDirection = normalize(vec3(0.5f, 0.5f, 0.5f));

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

void main()
{
	mat4 trans = Transform;
	vec3 sca = Scale;
	vec4 pain = vec4(ColorTint, 1.f) * VertexPaint;
	
	if (IsInstanced)
	{
		trans = InstanceTransform;
		sca = InstanceScale;
		pain = vec4(InstanceColor, 1.f) * VertexPaint;
	}
	
	data_out.VertexNormal = VertexNormal;
	float inten = sin((Time + VertexPosition.x + VertexPosition.z) * 3);
	//data_out.Paint = vec4(vec3(inten + 1, inten + 1, inten + 1) * 0.5, 1.f);
	data_out.Paint = pain;
	data_out.Transparency = InstanceTransparency;
	data_out.TextureUV = VertexUV;
	data_out.RenderMatrix = RenderMatrix;
	data_out.ModelPosition = (VertexPosition + WindDirection * ((1 - VertexUV.y) * (inten * 0.03 - 0.05) * 0.5)) * sca;
	data_out.WorldPosition = vec3(trans * vec4(data_out.ModelPosition, 1.0f));
	data_out.Transform = trans;
	data_out.RelativeToDirecLight = DirecLightProjection * vec4(data_out.WorldPosition, 1.f);
	data_out.CameraPosition = CameraPosition;
	
	gl_Position = vec4(data_out.WorldPosition, 1.f);
}
