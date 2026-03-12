// User Interface vertex shader, 12/03/2026
#version 460 core

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec4 VertexPaint;
layout (location = 3) in vec2 VertexUV;

uniform vec2 Position;
uniform vec2 Size;

out vec2 Frag_UV;

void main()
{
    gl_Position = vec4(vec3((VertexPosition.xy * 2.f) * Size + Position, -1.f), 1.f);
    Frag_UV = VertexUV;
}
