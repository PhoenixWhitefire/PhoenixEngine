// Particle.frag - Particles

#version 460 core

uniform sampler2D Image;

uniform float Transparency = 0.5f;

in vec2 FragIn_VertexPosition;

out vec4 FragColor;

void main()
{
	vec4 Color = texture(Image, FragIn_VertexPosition + vec2(0.5f, 0.5f));

	if (Color.a == 0.1f)
		//discard;

	FragColor = vec4(1.0f, 0.0f, 1.0f, 1.0f);
}
