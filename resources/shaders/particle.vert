// particle.vert - Particles

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

out vec3 Frag_VertexNormal;
out vec2 Frag_TextureUV;
out vec3 Frag_ColorTint;
out mat4 Frag_RenderMatrix;
out vec3 Frag_ModelPosition;
out vec3 Frag_WorldPosition;
out mat4 Frag_Transform;
out vec4 Frag_RelativeToDirecLight;

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

	Frag_VertexNormal = VertexNormal;
	Frag_ColorTint = col;
	Frag_TextureUV = VertexUV;
	Frag_RenderMatrix = RenderMatrix;

	Frag_ModelPosition = VertexPosition * sca;
	Frag_WorldPosition = vec3(trans * vec4(Frag_ModelPosition, 1.0f));
	Frag_Transform = trans;
	Frag_RelativeToDirecLight = DirecLightProjection * vec4(Frag_WorldPosition, 1.f);

	gl_Position = RenderMatrix * vec4(Frag_WorldPosition, 1.f);
}

