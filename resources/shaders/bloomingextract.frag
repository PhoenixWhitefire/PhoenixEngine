// 22/02/2025
// extract pixels above the specified threshold to be bloomed

#version 460 core

in vec2 Frag_UV;

out vec4 FragColor;

uniform sampler2D Texture;

uniform float Threshold = 1.f;

float getBrightness(vec3 Value)
{
	return dot(Value, vec3(0.299f, 0.587f, 0.114f));
}

void main()
{
	vec4 col = texture(Texture, Frag_UV);

    if (getBrightness(col.xyz) >= Threshold)
       FragColor = col;
    else
        FragColor = vec4(0.f, 0.f, 0.f, 1.f);
}
