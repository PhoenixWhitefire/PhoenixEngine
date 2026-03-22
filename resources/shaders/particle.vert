// particle.vert - Particles
#version 460 core

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec4 VertexPaint;
layout (location = 3) in vec2 VertexUV;

uniform mat4 RenderMatrix;
uniform vec3 Position;
uniform float Size;

out vec2 Frag_TextureUV;

void main()
{
	Frag_TextureUV = VertexUV;

	vec3 modelPosition = VertexPosition * vec3(Size);
	vec3 worldPosition = Position + modelPosition;

	gl_Position = RenderMatrix * vec4(worldPosition, 1.f);
}
