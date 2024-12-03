#version 460

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

out vec3 Frag_ModelPosition;
out vec3 Frag_WorldPosition;
out vec3 Frag_VertexNormal;
out vec3 Frag_ColorTint;
out vec2 Frag_TextureUV;
out mat4 Frag_Transform;
out vec4 Frag_RelativeToDirecLight;

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
} data_in[];

void main()
{
	gl_Position = data_in[0].RenderMatrix * gl_in[0].gl_Position;

	Frag_ModelPosition = data_in[0].ModelPosition;
	Frag_WorldPosition = data_in[0].WorldPosition;
	Frag_VertexNormal = data_in[0].VertexNormal;
	Frag_ColorTint = data_in[0].ColorTint;
	Frag_TextureUV = data_in[0].TextureUV;
	Frag_Transform = data_in[0].Transform;
	Frag_RelativeToDirecLight = data_in[0].RelativeToDirecLight;

	EmitVertex();

	gl_Position = data_in[1].RenderMatrix * gl_in[1].gl_Position;

	Frag_ModelPosition = data_in[1].ModelPosition;
	Frag_WorldPosition = data_in[1].WorldPosition;
	Frag_VertexNormal = data_in[1].VertexNormal;
	Frag_ColorTint = data_in[1].ColorTint;
	Frag_TextureUV = data_in[1].TextureUV;
	Frag_Transform = data_in[1].Transform;
	Frag_RelativeToDirecLight = data_in[1].RelativeToDirecLight;

	EmitVertex();

	gl_Position = data_in[2].RenderMatrix * gl_in[2].gl_Position;

	Frag_ModelPosition = data_in[2].ModelPosition;
	Frag_WorldPosition = data_in[2].WorldPosition;
	Frag_VertexNormal = data_in[2].VertexNormal;
	Frag_ColorTint = data_in[2].ColorTint;
	Frag_TextureUV = data_in[2].TextureUV;
	Frag_Transform = data_in[2].Transform;
	Frag_RelativeToDirecLight = data_in[2].RelativeToDirecLight;

	EmitVertex();

	EndPrimitive();
}
