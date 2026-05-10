// Skybox shader

#version 460 core

in vec3 FragIn_Direction;

out vec4 FragColor;

uniform samplerCube Phoenix_SkyboxCubemap;
uniform sampler2D Phoenix_SkyboxEquirectangular;
uniform bool Phoenix_IsSkyboxEquirectangular = false;
uniform bool Phoenix_HdrEnabled = false;
uniform bool Phoenix_DebugOverdraw = false;

uniform float Phoenix_Time = 0.f;

#define PI 3.14159265359

// https://discussions.unity.com/t/equirectangular-projection-shader-code/347527/3
vec2 RadialCoords(vec3 a_coords)
{
    vec3 a_coords_n = normalize(a_coords);
    float lon = atan(a_coords_n.z, a_coords_n.x);
    float lat = acos(a_coords_n.y);
    vec2 sphereCoords = vec2(lon, lat) * (1.0 / PI);
    return vec2(1 - (sphereCoords.x * 0.5 + 0.5), 1 - sphereCoords.y);
}

void main()
{
	if (Phoenix_DebugOverdraw)
	{
		FragColor = vec4(0.f, 0.f, 0.f, 1.f);
		return;
	}

	//FragColor = vec4(FragIn_Direction, 1.f);

	if (Phoenix_IsSkyboxEquirectangular)
	{
		vec2 equiUV = RadialCoords(FragIn_Direction);
		FragColor = vec4(texture(Phoenix_SkyboxEquirectangular, equiUV).xyz, 1.f);
	}
	else
	{
		FragColor = texture(Phoenix_SkyboxCubemap, FragIn_Direction);
	}
}
