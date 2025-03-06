#pragma once

#include "datatype/Color.hpp"
#include "datatype/Mesh.hpp"

#include "datatype/GameObject.hpp"
#include "gameobject/Base3D.hpp"

class Object_Mesh : public Object_Base3D
{
public:
	Object_Mesh();
	void Update(double) override;

	std::string GetRenderMeshPath();
	void SetRenderMesh(const std::string_view&);

	REFLECTION_DECLAREAPI;

private:
	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};;

	std::string m_MeshPath{};
	bool m_WaitingForMeshToLoad = false;
};