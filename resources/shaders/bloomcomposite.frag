// 22/02/2025
// combine all bloom mips into a single image

#version 460 core

in vec2 Frag_UV;

out vec4 FragColor;

uniform sampler2D Texture;

void main()
{
	vec3 col = vec3(0.f, 0.f, 0.f);

    for (int i = 5; i > 0; i--)
        col += textureLod(Texture, Frag_UV, i).xyz;

    FragColor = vec4(col, 1.f);
}
