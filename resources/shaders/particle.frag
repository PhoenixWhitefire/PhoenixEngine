// Particle.frag - Particles

#version 460 core

uniform sampler2D Image;

uniform float Transparency;
uniform vec3 Tint;

in vec2 Frag_TextureUV;

out vec4 FragColor;

void main()
{
	vec4 Color = texture(Image, Frag_TextureUV);
	FragColor = vec4(1.0f, 0.0f, 1.0f, 1.0f);
}
