// bloomseparated.frag, 05/10/2026

#version 460 core

in vec2 Frag_UV;
out vec4 FragColor;

uniform sampler2D Phoenix_BloomSource;
uniform float Phoenix_BloomScale;
uniform float Phoenix_BloomStdDeviation;
uniform bool Phoenix_BlurHorizontal;

// https://github.com/GarrettGunnell/AcerolaFX-Godot/blob/main/Effects/Blur/acerolafx_blur.gd
#define PI (3.14159265359f)

void main()
{
    ivec2 screenSize = textureSize(Phoenix_BloomSource, 0);
    float minAxis = min(float(screenSize.x), float(screenSize.y));
    float pixelScale = 1.f / minAxis;
    int kernelSize = int(minAxis * Phoenix_BloomScale * 0.01f);

    float twoSigmaSquared = 2.f * pow(Phoenix_BloomStdDeviation, 2.f);
    float gaussianDenominator = sqrt(PI * twoSigmaSquared);

    float totalWeight = 1.f / gaussianDenominator;
    vec3 sum = vec3(0.f);

    if (Phoenix_BlurHorizontal)
    {
        for (int x = -kernelSize; x <= kernelSize; x++)
        {
            if (x == 0)
                continue;

            float distSq = float(pow(x, 2));
            float gaussNumerator = exp(-distSq / twoSigmaSquared);
            float gaussian = gaussNumerator / gaussianDenominator;

            sum += texture(Phoenix_BloomSource, Frag_UV + vec2(x * pixelScale, 0.f)).xyz * gaussian;
            totalWeight += gaussian;
        }
    }
    else
    {
        for (int y = -kernelSize; y <= kernelSize; y++)
        {
            if (y == 0)
                continue;

            float distSq = float(pow(y, 2));
            float gaussNumerator = exp(-distSq / twoSigmaSquared);
            float gaussian = gaussNumerator / gaussianDenominator;

            sum += texture(Phoenix_BloomSource, Frag_UV + vec2(0.f, y * pixelScale)).xyz * gaussian;
            totalWeight += gaussian;
        }
    }

    FragColor = vec4(sum / vec3(totalWeight), 1.f);
}
