#version 460 core

in vec3 Frag_VertexNormal;

out vec4 FragColor;

void main()
{
  vec3 colorTint = Frag_VertexNormal * .5f + .5f;
  FragColor = vec4(colorTint, 1.f);
}