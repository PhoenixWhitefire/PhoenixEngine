// Uber shader for world geometry

#version 460 core

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec3 VertexColor;
layout (location = 3) in vec2 VertexUV;
// from Instanced Array
layout (location = 4) in mat4 InstanceTransform;
layout (location = 8) in vec3 InstanceScale;
layout (location = 9) in vec3 InstanceColor;

uniform mat4 RenderMatrix;

uniform mat4 Transform;
uniform vec3 Scale;
uniform vec3 ColorTint;
uniform bool IsInstanced;

uniform float Time;
uniform mat4 DirecLightProjection;

out vec3 Frag_ModelPosition;
out vec3 Frag_WorldPosition;
out mat4 Frag_Transform;
out vec4 Frag_RelativeToDirecLight;

out DATA
{
	vec3 VertexNormal;
	vec2 TextureUV;
	vec3 ColorTint;
	mat4 RenderMatrix;

	vec3 ModelPosition;
	vec3 WorldPosition;
	mat4 Transform;
	vec4 RelativeToDirecLight;
} data_out;

void main()
{
	mat4 trans = Transform;
	vec3 sca = Scale;
	vec3 col = ColorTint * VertexColor;

	if (IsInstanced)
	{
		trans = InstanceTransform;
		sca = InstanceScale;
		col = InstanceColor * VertexColor;
	}

	data_out.VertexNormal = VertexNormal;
	data_out.ColorTint = col;
	data_out.TextureUV = VertexUV;
	data_out.RenderMatrix = RenderMatrix;

	data_out.ModelPosition = VertexPosition * sca;
	data_out.WorldPosition = vec3(trans * vec4(data_out.ModelPosition, 1.0f));
	data_out.Transform = trans;
	data_out.RelativeToDirecLight = DirecLightProjection * vec4(data_out.WorldPosition, 1.f);

	gl_Position = vec4(data_out.WorldPosition, 1.f);
}
