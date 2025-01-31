#version 460 core

layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;

uniform mat4 RenderMatrix;
uniform mat4 Transform;

out vec3 Frag_VertexNormal;

void main()
{
  vec4 worldPosition = Transform * vec4(VertexPosition, 1.f);
  Frag_VertexNormal = VertexNormal;
  
  gl_Position = RenderMatrix * worldPosition;
}