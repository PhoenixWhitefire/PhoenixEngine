// Skybox shader

#version 460 core

in vec3 FragIn_Direction;

out vec4 FragColor;

uniform samplerCube SkyboxCubemap;
uniform bool HdrEnabled = false;
uniform bool DebugOverdraw = false;

void main()
{
	if (DebugOverdraw)
	{
		FragColor = vec4(0.f, 0.f, 0.f, 1.f);
		return;
	}
	
	//FragColor = vec4(FragIn_Direction, 1.f);

	FragColor = texture(SkyboxCubemap, FragIn_Direction);

	//if (HdrEnabled)
	//	FragColor = vec4(FragColor.xyz * 2.f, 1.f);
}
