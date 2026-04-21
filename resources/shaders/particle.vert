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
