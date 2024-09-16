#pragma once

#include<glm/matrix.hpp>

#include"datatype/GameObject.hpp"

#include"datatype/Color.hpp"
#include"datatype/Vector3.hpp"
#include"asset/Material.hpp"

enum class FaceCullingMode { None, BackFace, FrontFace };

class Object_Base3D : public GameObject
{
public:
	Object_Base3D();

	virtual uint32_t GetRenderMeshId();

	glm::mat4 Transform = glm::mat4(1.0f);
	Vector3 Size = Vector3(1.0f, 1.0f, 1.0f);

	Vector3 LinearVelocity;
	Vector3 AngularVelocity;
	double Mass = 1.0f;

	float Transparency = 0.0f;
	float Reflectivity = 0.0f;

	Color ColorRGB = Color(1.0f, 1.0f, 1.0f);

	RenderMaterial* Material;

	bool ComputePhysics = false;

	FaceCullingMode FaceCulling = FaceCullingMode::BackFace;

	PHX_GAMEOBJECT_API_REFLECTION;

protected:
	uint32_t m_RenderMeshId{};

private:
	static void s_DeclareReflections();
	static inline Api s_Api{};
};
