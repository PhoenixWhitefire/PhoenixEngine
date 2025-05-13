// Skybox shader

#version 460 core

in vec3 FragIn_Direction;

out vec4 FragColor;

uniform samplerCube SkyboxCubemap;
uniform bool HdrEnabled = false;
uniform bool DebugOverdraw = false;

uniform float Time = 0.f;

void main()
{
	if (DebugOverdraw)
	{
		FragColor = vec4(0.f, 0.f, 0.f, 1.f);
		return;
	}
	
	//FragColor = vec4(FragIn_Direction, 1.f);

	FragColor = texture(SkyboxCubemap, FragIn_Direction);
	
	vec4 realCol = FragColor;
	
	/*if (Time > 10.f)
	{
	        float t = Time - 5.f;
	        
		FragColor *= mix(vec4(1.f, 1.f, 1.f, 1.f), vec4(0.25f, 0.f, 0.f, 1.f), clamp((t - 5.f) / 12, 0.f, 1.f));
		
		float swayMul = clamp(clamp(t - 16.f, 0.f, t) / 16.f, 0.f, 1.f);
		float contrastChangeAlpha = clamp(clamp((t - 12.5f) + sin(t) * 1.7f * swayMul, 0.f, t) / 12.f, 0.f, 1.f);
		
		if (length(vec3(realCol)) > contrastChangeAlpha + 0.5f + (sin(t) * 0.1f))
		{
			FragColor *= mix(vec4(1.f, 1.f, 1.f, 1.f), vec4(3.f, 0.1f, 0.1f, 1.f), contrastChangeAlpha);
		}
		else
		{
		        FragColor *= mix(vec4(1.f, 1.f, 1.f, 1.f), vec4(0.5f, 0.5f, 0.5f, 1.f), contrastChangeAlpha);
		}
	}*/

	//if (HdrEnabled)
	//	FragColor = vec4(FragColor.xyz * 2.f, 1.f);
}
