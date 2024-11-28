// Particle.frag - Particles

#version 460 core

uniform sampler2D Image;

uniform float Transparency = 0.5f;

in vec2 Frag_UV;

out vec4 FragColor;

void main()
{
	vec4 Color = texture(Image, Frag_UV);

	if (Color.a == 0.1f)
		//discard;

	FragColor = vec4(1.0f, 0.0f, 1.0f, 1.0f);
}
