// 22/02/2025
// blur in one direction

#version 460 core

in vec2 Frag_UV;

out vec4 FragColor;

uniform sampler2D Texture;

uniform float BlurRadius = 2;
uniform bool BlurXAxis;
uniform int LodLevel = 0;

// https://stackoverflow.com/a/64845819
void main()
{
    float x, y, rr = BlurRadius*BlurRadius, d, w, w0;
    ivec2 size = textureSize(Texture, LodLevel);

    vec2 p = Frag_UV;
    vec4 col = vec4(0.f, 0.f, 0.f, 0.f);

    w0 = 0.5135f / pow(BlurRadius, .96f);

    if (BlurXAxis)
        for (d = 1.f/size.x, x = -BlurRadius, p.x += x*d; x <= BlurRadius; x++, p.x += d)
        {
            w = w0 * exp((-x*x) / (2.f*rr));
            col += textureLod(Texture, p, LodLevel) * w;
        }
    else
        for (d = 1.f/size.y, y = -BlurRadius, p.y += y*d; y <= BlurRadius; y++, p.y += d)
        {
            w = w0 * exp((-y*y) / (2.f*rr));
            col += textureLod(Texture, p, LodLevel) * w;
        }
    
    FragColor = col;
}
