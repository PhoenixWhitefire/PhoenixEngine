// Skybox shader

#version 460 core

in vec3 FragIn_Direction;

out vec4 FragColor;

uniform samplerCube SkyboxCubemap;

void main()
{
	//FragColor = vec4(FragIn_Direction, 1.f);
	FragColor = texture(SkyboxCubemap, vec3(FragIn_Direction.x, -FragIn_Direction.y, FragIn_Direction.z));
}
