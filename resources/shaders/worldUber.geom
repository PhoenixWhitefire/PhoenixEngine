#version 460

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

const int MAX_LIGHTS = 6;

struct LightObject
{
	int Type;
	bool Shadows;

	// generic, applies to all light types (i.e., directional, point, spot lights)
	vec3 Position;
	vec3 Color;

	// point lights and spotlights
	float Range;
	// spotlights
	float Angle;
};

out vec3 Frag_ModelPosition;
out vec3 Frag_WorldPosition;
out vec3 Frag_VertexNormal;
out vec3 Frag_ColorTint;
out vec2 Frag_TextureUV;
out mat4 Frag_Transform;
out vec4 Frag_RelativeToDirecLight;
//out mat3 Frag_TBN;
out vec3 Frag_CameraPosition;

in DATA
{
	vec3 VertexNormal;
	vec2 TextureUV;
	vec3 ColorTint;
	mat4 RenderMatrix;

	vec3 ModelPosition;
	vec3 WorldPosition;
	mat4 Transform;
	vec4 RelativeToDirecLight;
	vec3 CameraPosition;
} data_in[];

void main()
{
	/*
	vec3 edge0 = gl_in[1].gl_Position.xyz - gl_in[0].gl_Position.xyz;
	vec3 edge1 = gl_in[2].gl_Position.xyz - gl_in[0].gl_Position.xyz;
	vec2 deltaUV0 = data_in[1].TextureUV - data_in[0].TextureUV;
	vec2 deltaUV1 = data_in[2].TextureUV - data_in[0].TextureUV;
	
	float invDet = 1.f / (deltaUV0.x * deltaUV1.y - deltaUV1.x * deltaUV0.y);
	
	vec3 tangent = vec3(invDet * (deltaUV1.y * edge0 - deltaUV0.y * edge1));
	vec3 bitangent = vec3(invDet * (-deltaUV1.x * edge0 - deltaUV0.x * edge1));
	
	vec3 T = normalize(vec3(data_in[0].Transform * vec4(tangent, 0.f)));
	vec3 B = normalize(vec3(data_in[0].Transform * vec4(bitangent, 0.f)));
	vec3 N = data_in[0].VertexNormal; //normalize(vec3(data_in[0].Transform * vec4(cross(edge1, edge0), 0.f)));
	
	mat3 TBN = mat3(T, B, N);
	//TBN = transpose(TBN);
	*/
	
	gl_Position = data_in[0].RenderMatrix * gl_in[0].gl_Position;

	Frag_ModelPosition = data_in[0].ModelPosition;
	Frag_WorldPosition = data_in[0].WorldPosition;
	Frag_VertexNormal = data_in[0].VertexNormal;
	Frag_ColorTint = data_in[0].ColorTint;
	Frag_TextureUV = data_in[0].TextureUV;
	Frag_Transform = data_in[0].Transform;
	Frag_RelativeToDirecLight = data_in[0].RelativeToDirecLight;
	//Frag_TBN = TBN;
	Frag_CameraPosition = data_in[0].CameraPosition;
	
	EmitVertex();

	gl_Position = data_in[1].RenderMatrix * gl_in[1].gl_Position;

	Frag_ModelPosition = data_in[1].ModelPosition;
	Frag_WorldPosition = data_in[1].WorldPosition;
	Frag_VertexNormal = data_in[1].VertexNormal;
	Frag_ColorTint = data_in[1].ColorTint;
	Frag_TextureUV = data_in[1].TextureUV;
	Frag_Transform = data_in[1].Transform;
	Frag_RelativeToDirecLight = data_in[1].RelativeToDirecLight;
	//Frag_TBN = TBN;
	Frag_CameraPosition = data_in[1].CameraPosition;
	
	EmitVertex();

	gl_Position = data_in[2].RenderMatrix * gl_in[2].gl_Position;

	Frag_ModelPosition = data_in[2].ModelPosition;
	Frag_WorldPosition = data_in[2].WorldPosition;
	Frag_VertexNormal = data_in[2].VertexNormal;
	Frag_ColorTint = data_in[2].ColorTint;
	Frag_TextureUV = data_in[2].TextureUV;
	Frag_Transform = data_in[2].Transform;
	Frag_RelativeToDirecLight = data_in[2].RelativeToDirecLight;
	//Frag_TBN = TBN;
	Frag_CameraPosition = data_in[2].CameraPosition;
	
	EmitVertex();

	EndPrimitive();
}
