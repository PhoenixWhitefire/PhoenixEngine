#pragma once

#include<datatype/GameObject.hpp>
#include<datatype/Mesh.hpp>

#include<datatype/Color.hpp>
#include<datatype/Vector3.hpp>
#include<render/Material.hpp>

#include<glm/matrix.hpp>

class Object_Base3D : public GameObject {
public:

	Object_Base3D();

	std::string ClassName = "Base3D";

	virtual Mesh* GetRenderMesh();
	virtual void SetRenderMesh(Mesh);

	glm::mat4 Matrix = glm::mat4(1.0f);
	Vector3 Size = Vector3(1.0f, 1.0f, 1.0f);

	Vector3 LinearVelocity;
	Vector3 AngularVelocity;
	float Mass = 1.0f;

	float Transparency = 0.0f;
	float Reflectivity = 0.0f;

	Color ColorRGB = Color(1.0f, 1.0f, 1.0f);

	RenderMaterial* Material;

	bool ComputePhysics = false;

protected:
	Mesh RenderMesh;
};
