// User Interface fragment shader, 12/03/2026
#version 460 core

uniform vec3 Phoenix_BackgroundColor;
uniform float Phoenix_BackgroundTransparency;
uniform bool Phoenix_IsImage = false;
uniform sampler2D Phoenix_Image;

in vec2 Frag_UV;
out vec4 FragColor;

void main()
{
    if (!Phoenix_IsImage)
    {
        FragColor = vec4(Phoenix_BackgroundColor, 1.f - Phoenix_BackgroundTransparency);
    }
    else
    {
        vec4 imageCol = texture(Phoenix_Image, Frag_UV);
        FragColor = vec4(vec3(imageCol) * Phoenix_BackgroundColor, imageCol.a - Phoenix_BackgroundTransparency);
    }

    FragColor = vec4(pow(FragColor.xyz, vec3(1.f / 2.2f)), FragColor.a);
}
