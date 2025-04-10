/*

#include "gameobject/Base3D.hpp"
#include "asset/MaterialManager.hpp"
#include "asset/MeshProvider.hpp"

PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(Base3D);

static bool s_DidInitReflection = false;

void Object_Base3D::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);

	REFLECTION_DECLAREPROP(
		"Transform",
		Matrix,
		[](Reflection::Reflectable* p)
		{
			return Reflection::GenericValue(static_cast<Object_Base3D*>(p)->Transform);
		},
		[](Reflection::Reflectable* p, const Reflection::GenericValue& gv)
		{
			Object_Base3D* obj = static_cast<Object_Base3D*>(p);
			obj->Transform = gv.AsMatrix();

			obj->RecomputeAabb();
		}
	);

	REFLECTION_DECLAREPROP(
		"Size",
		Vector3,
		[](Reflection::Reflectable* p)
		{
			return static_cast<Object_Base3D*>(p)->Size.ToGenericValue();
		},
		[](Reflection::Reflectable* p, const Reflection::GenericValue& gv)
		{
			Object_Base3D* obj = static_cast<Object_Base3D*>(p);
			obj->Size = gv;

			obj->RecomputeAabb();
		}
	);

	REFLECTION_DECLAREPROP(
		"Material",
		String,
		[](Reflection::Reflectable* g)
		{
			Object_Base3D* p = static_cast<Object_Base3D*>(g);
			MaterialManager* mtlManager = MaterialManager::Get();

			return mtlManager->GetMaterialResource(p->MaterialId).Name;
		},
		[](Reflection::Reflectable* g, Reflection::GenericValue gv)
		{
			Object_Base3D* p = static_cast<Object_Base3D*>(g);
			MaterialManager* mtlManager = MaterialManager::Get();

			p->MaterialId = mtlManager->LoadFromPath(gv.AsStringView());
		}
	);

	REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(Object_Base3D, Tint, Color);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Base3D, CastsShadows, Bool);
	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_Base3D, Transparency, Double, float);
	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_Base3D, MetallnessFactor, Double, float);
	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_Base3D, RoughnessFactor, Double, float);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Base3D, PhysicsDynamics, Bool);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Base3D, PhysicsCollisions, Bool);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Base3D, Density, Double);
	REFLECTION_DECLAREPROP_SIMPLE(Object_Base3D, Friction, Double);

	REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(Object_Base3D, LinearVelocity, Vector3);

	REFLECTION_DECLAREPROP(
		"FaceCulling",
		Integer,
		[](Reflection::Reflectable* g)
		{
			Object_Base3D* p = static_cast<Object_Base3D*>(g);
			return (int)p->FaceCulling;
		},
		[](Reflection::Reflectable* g, Reflection::GenericValue gv)
		{
			Object_Base3D* p = static_cast<Object_Base3D*>(g);
			p->FaceCulling = (FaceCullingMode)gv.AsInteger();
		}
	);
}

Object_Base3D::Object_Base3D()
{
	this->Name = "Base3D";
	this->ClassName = "Base3D";

	this->RenderMeshId = MeshProvider::Get()->LoadFromPath("!Cube");
	this->MaterialId = MaterialManager::Get()->LoadFromPath("plastic");

	s_DeclareReflections();
	ApiPointer = &s_Api;
}

void Object_Base3D::RecomputeAabb()
{
	glm::vec3 glmsize = (glm::vec3)this->Size;

	glm::vec3 a = this->Transform * glm::vec4(glmsize * glm::vec3( 1.f,  1.f,  1.f), 1.f);
	glm::vec3 b = this->Transform * glm::vec4(glmsize * glm::vec3(-1.f, -1.f, -1.f), 1.f);

	glm::vec3 c = this->Transform * glm::vec4(glmsize * glm::vec3(-1.f,  1.f,  1.f), 1.f);
	glm::vec3 d = this->Transform * glm::vec4(glmsize * glm::vec3( 1.f, -1.f,  1.f), 1.f);
	glm::vec3 e = this->Transform * glm::vec4(glmsize * glm::vec3( 1.f,  1.f, -1.f), 1.f);

	glm::vec3 f = this->Transform * glm::vec4(glmsize * glm::vec3(-1.f, -1.f,  1.f), 1.f);
	glm::vec3 g = this->Transform * glm::vec4(glmsize * glm::vec3( 1.f, -1.f, -1.f), 1.f);

	glm::vec3 h = this->Transform * glm::vec4(glmsize * glm::vec3(-1.f,  1.f, -1.f), 1.f);

	std::array<glm::vec3, 8> verts = { a, b, c, d, e, f, g, h };

	glm::vec3 max{ FLT_MIN, FLT_MIN, FLT_MIN };
	glm::vec3 min{ FLT_MAX, FLT_MAX, FLT_MAX };

	for (const glm::vec3& v : verts)
	{
		if (v.x > max.x)
			max.x = v.x;
		if (v.y > max.y)
			max.y = v.y;
		if (v.z > max.z)
			max.z = v.z;

		if (v.x < min.x)
			min.x = v.x;
		if (v.y < min.y)
			min.y = v.y;
		if (v.z < min.z)
			min.z = v.z;
	}

	glm::vec3 size = (max - min) / 2.f;
	glm::vec3 center = (min + max) / 2.f;

	this->CollisionAabb.Position = center;
	this->CollisionAabb.Size = size;
}
*/
