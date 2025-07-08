#include "component/Mesh.hpp"
#include "component/Transform.hpp"

#include "asset/MaterialManager.hpp"
#include "asset/MeshProvider.hpp"
#include "component/Bone.hpp"

class MeshManager : public BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();

		if (!Object->GetComponent<EcTransform>())
			Object->AddComponent(EntityComponent::Transform);
		
		m_Components.back().MaterialId = MaterialManager::Get()->LoadFromPath("plastic");
		m_Components.back().Object = Object;
		
        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    virtual std::vector<void*> GetComponents() override
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (EcMesh& t : m_Components)
            v.push_back((void*)&t);
        
        return v;
    }

	virtual void* GetComponent(uint32_t Id) override
	{
		return &m_Components[Id];
	}

    virtual void DeleteComponent(uint32_t Id) override
    {
        // TODO id reuse with handles that have a counter per re-use to reduce memory growth

		m_Components[Id].Object.Invalidate();
    }

	virtual void Shutdown() override
    {
        for (size_t i = 0; i < m_Components.size(); i++)
            DeleteComponent(i);
    }

    virtual const Reflection::PropertyMap& GetProperties() override
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

			EC_PROP(
				"CollisionFidelity",
				Integer,
				[](void* p)
				-> Reflection::GenericValue
				{
					return static_cast<uint32_t>(static_cast<EcMesh*>(p)->CollisionFidelity);
				},
				[](void* p, const Reflection::GenericValue& gv)
				-> void
				{
					static_cast<EcMesh*>(p)->CollisionFidelity = static_cast<CollisionFidelityMode>(gv.AsInteger());
				}
			),

			EC_PROP_SIMPLE(EcMesh, Transparency, Double),
			EC_PROP_SIMPLE(EcMesh, MetallnessFactor, Double),
			EC_PROP_SIMPLE(EcMesh, RoughnessFactor, Double),

			EC_PROP_SIMPLE_NGV(EcMesh, Tint, Color),

			EC_PROP_SIMPLE(EcMesh, LinearVelocity, Vector3),
			EC_PROP_SIMPLE(EcMesh, AngularVelocity, Vector3),

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

    virtual const Reflection::FunctionMap& GetFunctions() override
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

void EcMesh::SetRenderMesh(const std::string_view& MeshPath)
{
	MeshProvider* meshProvider = MeshProvider::Get();
	GameObjectRef obj = Object;

	this->RenderMeshId = meshProvider->LoadFromPath(
		MeshPath,
		true,
		false,
		[obj](Mesh& mesh)
		{
			if (mesh.Bones.size() == 0)
				return;
			
			for (GameObject* ch : obj->GetChildren())
				if (ch->GetComponent<EcBone>())
					ch->Destroy();

			for (uint8_t boneId = 0; boneId < mesh.Bones.size(); boneId++)
			{
				const Bone& b = mesh.Bones[boneId];
			
				GameObject* boneObj = GameObject::Create(EntityComponent::Bone);
				EcBone* bone = boneObj->GetComponent<EcBone>();

				boneObj->SetParent(obj);
				boneObj->Name = b.Name;
				boneObj->Serializes = false;
				
				bone->Transform = b.Transform;
				bone->SkeletalBoneId = boneId;
			}
		}
	);
	this->Asset = MeshPath;
}

void EcMesh::RecomputeAabb()
{
	EcTransform* ct = this->Object->GetComponent<EcTransform>();
	const glm::mat4& transform = ct->Transform;
	const glm::vec3& glmsize = ct->Size;

	if (CollisionFidelity == CollisionFidelityMode::Aabb)
	{
		std::array<glm::vec3, 8> verts;
		
		int i = 0;
		for (int x : {-1, 1})
			for (int y : {-1, 1})
				for (int z : {-1, 1})
					verts[i++] = transform * glm::vec4(glmsize * glm::vec3(x, y, z), 1.f);

		glm::vec3 max( -FLT_MAX );
		glm::vec3 min(  FLT_MAX );

		for (const glm::vec3& v : verts)
		{
			max = glm::max(max, v);
			min = glm::min(min, v);
		}
		
		glm::vec3 size = (max - min) / 2.f;
		glm::vec3 center = (min + max) / 2.f;

		this->CollisionAabb.Position = center;
		this->CollisionAabb.Size = size;
	}
	else if (CollisionFidelity == CollisionFidelityMode::AabbStaticSize)
	{
		this->CollisionAabb.Position = glm::vec3(transform[3]);
		this->CollisionAabb.Size = glmsize;
	}
}
