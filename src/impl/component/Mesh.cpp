#include "component/Mesh.hpp"
#include "component/Transform.hpp"

#include "asset/MaterialManager.hpp"
#include "asset/MeshProvider.hpp"
#include "component/Bone.hpp"

class MeshManager : BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) final
    {
        m_Components.emplace_back();

		if (!Object->GetComponent<EcTransform>())
			Object->AddComponent(EntityComponent::Transform);
		
		m_Components.back().MaterialId = MaterialManager::Get()->LoadFromPath("plastic");
		m_Components.back().Object = Object;
		
        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    virtual std::vector<void*> GetComponents() final
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (const EcMesh& t : m_Components)
            v.push_back((void*)&t);
        
        return v;
    }

	virtual void* GetComponent(uint32_t Id) final
	{
		return &m_Components[Id];
	}

    virtual void DeleteComponent(uint32_t Id) final
    {
        // TODO id reuse with handles that have a counter per re-use to reduce memory growth
    }

    virtual const Reflection::PropertyMap& GetProperties() final
    {
        static const Reflection::PropertyMap props = 
        {
            EC_PROP_SIMPLE(EcMesh, PhysicsDynamics, Boolean),
			EC_PROP_SIMPLE(EcMesh, PhysicsCollisions, Boolean),
			EC_PROP_SIMPLE(EcMesh, CastsShadows, Boolean),

			EC_PROP(
				"FaceCulling",
				Integer,
				[](void* p)
				-> Reflection::GenericValue
				{
					return static_cast<uint32_t>(static_cast<EcMesh*>(p)->FaceCulling);
				},
				[](void* p, const Reflection::GenericValue& gv)
				-> void
				{
					static_cast<EcMesh*>(p)->FaceCulling = static_cast<FaceCullingMode>(gv.AsInteger());
				}
			),

			EC_PROP_SIMPLE(EcMesh, Transparency, Double),
			EC_PROP_SIMPLE(EcMesh, MetallnessFactor, Double),
			EC_PROP_SIMPLE(EcMesh, RoughnessFactor, Double),

			EC_PROP_SIMPLE_NGV(EcMesh, Tint, Color),

			EC_PROP_SIMPLE_NGV(EcMesh, LinearVelocity, Vector3),
			EC_PROP_SIMPLE_NGV(EcMesh, AngularVelocity, Vector3),

			EC_PROP_SIMPLE(EcMesh, Density, Double),
			EC_PROP_SIMPLE(EcMesh, Friction, Double),

			EC_PROP(
				"Asset",
				String,
				EC_GET_SIMPLE(EcMesh, Asset),
				[](void* p, const Reflection::GenericValue& gv)
				{
					EcMesh* mesh = static_cast<EcMesh*>(p);
					mesh->SetRenderMesh(gv.AsStringView());
				}
			),

			EC_PROP(
				"Material",
				String,
				[](void* p)
				-> Reflection::GenericValue
				{
					EcMesh* m = static_cast<EcMesh*>(p);
					MaterialManager* mtlManager = MaterialManager::Get();
				
					return mtlManager->GetMaterialResource(m->MaterialId).Name;
				},
				[](void* p, Reflection::GenericValue gv)
				{
					EcMesh* m = static_cast<EcMesh*>(p);
					MaterialManager* mtlManager = MaterialManager::Get();
				
					m->MaterialId = mtlManager->LoadFromPath(gv.AsStringView());
				}
			)
        };

        return props;
    }

    virtual const Reflection::FunctionMap& GetFunctions() final
    {
        static const Reflection::FunctionMap funcs = {};
        return funcs;
    }

    MeshManager()
    {
        GameObject::s_ComponentManagers[(size_t)EntityComponent::Mesh] = this;
    }

private:
    std::vector<EcMesh> m_Components;
};

static inline MeshManager Instance{};

void EcMesh::SetRenderMesh(const std::string_view& Mesh)
{
	MeshProvider* meshProvider = MeshProvider::Get();

	this->RenderMeshId = meshProvider->LoadFromPath(Mesh);

	// in case it has any bones
	// can't do this in this function because it'll (most likely)
	// load asynchronously
	this->WaitingForMeshToLoad = true;
	this->Asset = Mesh;
}

void EcMesh::RecomputeAabb()
{
	EcTransform* ct = this->Object->GetComponent<EcTransform>();
	const glm::mat4& transform = ct->Transform;
	const glm::vec3& glmsize = ct->Size;

	glm::vec3 a = transform * glm::vec4(glmsize * glm::vec3( 1.f,  1.f,  1.f), 1.f);
	glm::vec3 b = transform * glm::vec4(glmsize * glm::vec3(-1.f, -1.f, -1.f), 1.f);

	glm::vec3 c = transform * glm::vec4(glmsize * glm::vec3(-1.f,  1.f,  1.f), 1.f);
	glm::vec3 d = transform * glm::vec4(glmsize * glm::vec3( 1.f, -1.f,  1.f), 1.f);
	glm::vec3 e = transform * glm::vec4(glmsize * glm::vec3( 1.f,  1.f, -1.f), 1.f);

	glm::vec3 f = transform * glm::vec4(glmsize * glm::vec3(-1.f, -1.f,  1.f), 1.f);
	glm::vec3 g = transform * glm::vec4(glmsize * glm::vec3( 1.f, -1.f, -1.f), 1.f);

	glm::vec3 h = transform * glm::vec4(glmsize * glm::vec3(-1.f,  1.f, -1.f), 1.f);

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

/*
PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(Mesh);

static bool s_DidInitReflection = false;

void Object_Mesh::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_INHERITAPI(Base3D);

	REFLECTION_DECLAREPROP(
		"Asset",
		String,
		[](Reflection::Reflectable* p)
		{
			return static_cast<Object_Mesh*>(p)->GetRenderMeshPath();
		},
		[](Reflection::Reflectable* p, const Reflection::GenericValue& gv)
		{
			static_cast<Object_Mesh*>(p)->SetRenderMesh(gv.AsStringView());
		}
	);
}

Object_Mesh::Object_Mesh()
{
	this->Name = "Mesh";
	this->ClassName = "Mesh";

	this->SetRenderMesh("!Cube");

	s_DeclareReflections();
	ApiPointer = &s_Api;
}

void Object_Mesh::Update(double)
{
	if (!m_WaitingForMeshToLoad)
		return;

	MeshProvider* meshProvider = MeshProvider::Get();

	const Mesh& mesh = meshProvider->GetMeshResource(this->RenderMeshId);

	if (mesh.GpuId == UINT32_MAX)
		return;

	for (GameObject* ch : this->GetChildren())
		if (ch->ClassName == "Bone")
			ch->Destroy();

	for (uint8_t boneId = 0; boneId < mesh.Bones.size(); boneId++)
	{
		const Bone& b = mesh.Bones[boneId];

		Object_Bone* bone = static_cast<Object_Bone*>(GameObject::Create("Bone"));
		bone->SetParent(this);

		bone->LocalTransform = b.Transform;
		bone->SkeletalBoneId = boneId;
		bone->Name = b.Name;
		bone->Serializes = false;
	}

	m_WaitingForMeshToLoad = false;
}

void Object_Mesh::SetRenderMesh(const std::string_view& NewRenderMesh)
{
	MeshProvider* meshProvider = MeshProvider::Get();

	this->RenderMeshId = meshProvider->LoadFromPath(NewRenderMesh);
	m_MeshPath = NewRenderMesh;

	// in case it has any bones
	// can't do this in this function because it'll (most likely)
	// load asynchronously
	m_WaitingForMeshToLoad = true;
}

std::string Object_Mesh::GetRenderMeshPath()
{
	return m_MeshPath;
}
*/
