#version 330 core

in vec3 FragIn_Direction;

out vec4 FragColor;

uniform samplerCube SkyCubemap;

void main()
{
	FragColor = texture(SkyCubemap, FragIn_Direction);
}
