// Particle.frag - Particles

#version 460 core

uniform sampler2D Phoenix_Image;

uniform float Phoenix_Transparency;
uniform vec3 Phoenix_Tint = vec3(1.f, 1.f, 1.f);

in vec2 Frag_TextureUV;

out vec4 FragColor;

void main()
{
	vec4 color = texture(Phoenix_Image, Frag_TextureUV);
	color.a -= Phoenix_Transparency;

	if (color.a < 0.05f)
		discard;

	FragColor = vec4(color.rgb, color.a);
}
