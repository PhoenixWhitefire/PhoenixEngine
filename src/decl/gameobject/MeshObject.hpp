#pragma once

#include "datatype/Color.hpp"
#include "datatype/Mesh.hpp"

#include "datatype/GameObject.hpp"
#include "gameobject/Base3D.hpp"

class Object_Mesh : public Object_Base3D
{
public:
	Object_Mesh();

	std::string GetRenderMeshPath();
	void SetRenderMesh(const std::string&);

	bool HasTransparency = false;

	PHX_GAMEOBJECT_API_REFLECTION;

private:
	static void s_DeclareReflections();
	static inline Api s_Api{};

	std::string m_MeshPath{};
};