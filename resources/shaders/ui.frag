// User Interface fragment shader, 12/03/2026
#version 460 core

uniform vec3 BackgroundColor;
uniform float BackgroundTransparency;
uniform bool IsImage = false;
uniform sampler2D Image;

in vec2 Frag_UV;
out vec4 FragColor;

void main()
{
    if (!IsImage)
    {
        FragColor = vec4(BackgroundColor, 1.f - BackgroundTransparency);
    }
    else
    {
        vec4 imageCol = texture(Image, Frag_UV);
        FragColor = vec4(vec3(imageCol) * BackgroundColor, imageCol.a - BackgroundTransparency);
    }
}
