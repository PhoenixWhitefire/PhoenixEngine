// bloomextract.frag, 05/10/2026
#version 460 core

in vec2 Frag_UV;
out vec4 FragColor;

uniform sampler2D Phoenix_PostProcessBuffer;
uniform float Phoenix_BloomThreshold;

void main()
{
	vec4 col = texture(Phoenix_PostProcessBuffer, Frag_UV);

    if (col.r * 0.2126f + col.g * 0.7152f + col.b * 0.0722f >= Phoenix_BloomThreshold)
    {
        FragColor = col;
    }
    else
    {
        FragColor = vec4(0.f);
    }
}
