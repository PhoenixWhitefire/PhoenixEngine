#pragma once

#include"datatype/Color.hpp"
#include"datatype/Mesh.hpp"

#include"datatype/GameObject.hpp"
#include"gameobject/Base3D.hpp"

class Object_Mesh : public Object_Base3D
{
public:
	Object_Mesh();

	void SetRenderMesh(Mesh);

	std::string Asset;
	bool HasTransparency = false;

private:
	std::vector<Vertex> BlankVertices;
	std::vector<uint32_t> BlankIndices;

	static RegisterDerivedObject<Object_Mesh> RegisterClassAs;
	static void s_DeclareReflections();
	static bool s_DidInitReflection;
};