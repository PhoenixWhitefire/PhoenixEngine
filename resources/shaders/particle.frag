// Particle.frag - Particles

#version 460 core

uniform sampler2D Image;

uniform float Transparency;
uniform vec3 Tint = vec3(1.f, 1.f, 1.f);

in vec2 Frag_TextureUV;

out vec4 FragColor;

void main()
{
	vec4 color = texture(Image, Frag_TextureUV);
	color.a -= Transparency;

	if (color.a < 0.05f)
		discard;

	FragColor = vec4(color.rgb, color.a);
}
