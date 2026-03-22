// particle.vert - Particles
#version 460 core

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec3 VertexColor;
layout (location = 3) in vec2 VertexUV;

uniform mat4 RenderMatrix;
uniform mat4 Transform;
uniform float Size;

out vec2 Frag_TextureUV;
out vec3 Frag_Tint;

void main()
{
	Frag_TextureUV = VertexUV;

	vec3 modelPosition = VertexPosition;// * vec3(Size);
	vec3 worldPosition = vec3(Transform * vec4(modelPosition, 1.0f));

	gl_Position = RenderMatrix * vec4(worldPosition, 1.f);
}
