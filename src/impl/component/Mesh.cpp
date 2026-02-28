#include <tracy/Tracy.hpp>

#include "component/Mesh.hpp"
#include "component/Transform.hpp"

#include "asset/MaterialManager.hpp"
#include "asset/MeshProvider.hpp"
#include "component/Workspace.hpp"
#include "component/RigidBody.hpp"
#include "component/Bone.hpp"

static std::unordered_map<std::string, std::vector<uint32_t>> FreeSkinnedMeshPseudoAssets;

static void tryMarkFreeSkinnedMeshPseudoAsset(EcMesh& mesh)
{
	if (Mesh& meshAsset = MeshProvider::Get()->GetMeshResource(mesh.RenderMeshId);
		meshAsset.Bones.size() > 0
	)
	{
		FreeSkinnedMeshPseudoAssets[mesh.Asset].push_back(mesh.RenderMeshId);

		MeshProvider::GpuMesh& gpuMesh = MeshProvider::Get()->GetGpuMesh(meshAsset.GpuId);

		// reset all vertex transformations
		for (glm::mat4& t : gpuMesh.SkinningData)
			t = glm::mat4(1.f);
	}
}

class MeshManager : public ComponentManager<EcMesh>
{
public:
    uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();

		uint32_t id = static_cast<uint32_t>(m_Components.size() - 1);

		EcMesh& cm = m_Components.back();
		cm.MaterialId = MaterialManager::Get()->LoadFromPath("plastic");
		cm.RenderMeshId = MeshProvider::Get()->LoadFromPath("!Cube");
		cm.ComponentId = id;
		cm.Object = Object;
		
        return id;
    }

    void DeleteComponent(uint32_t Id) override
    {
        // TODO id reuse with handles that have a counter per re-use to reduce memory growth

		EcMesh& mesh = m_Components[Id];
		tryMarkFreeSkinnedMeshPseudoAsset(mesh);

		ComponentManager<EcMesh>::DeleteComponent(Id);
    }

	virtual Reflection::GenericValue GetDefaultPropertyValue(const std::string_view& Property) override
	{
		return GetDefaultPropertyValue(&GetProperties().at(Property));
	}

	virtual Reflection::GenericValue GetDefaultPropertyValue(const Reflection::PropertyDescriptor* Property) override
	{
		static EcMesh Defaults;
		Defaults.MaterialId = MaterialManager::Get()->LoadFromPath("plastic");
		Defaults.RenderMeshId = MeshProvider::Get()->LoadFromPath("!Cube");

		return Property->Get((void*)&Defaults);
	}

    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = 
        {
			REFLECTION_PROPERTY_SIMPLE(EcMesh, CastsShadows, Boolean),

			REFLECTION_PROPERTY(
				"FaceCulling",
				Integer,
				[](void* p)
				-> Reflection::GenericValue
				{
					return static_cast<uint32_t>(static_cast<EcMesh*>(p)->FaceCulling);
				},
				[](void* p, const Reflection::GenericValue& gv)
				{
					static_cast<EcMesh*>(p)->FaceCulling = static_cast<FaceCullingMode>(gv.AsInteger());
				}
			),

			REFLECTION_PROPERTY_SIMPLE(EcMesh, Transparency, Double),
			REFLECTION_PROPERTY_SIMPLE(EcMesh, MetallnessFactor, Double),
			REFLECTION_PROPERTY_SIMPLE(EcMesh, RoughnessFactor, Double),
			REFLECTION_PROPERTY_SIMPLE_NGV(EcMesh, Tint, Color),

			REFLECTION_PROPERTY(
				"MeshAsset",
				String,
				REFLECTION_PROPERTY_GET_SIMPLE(EcMesh, Asset),
				[](void* p, const Reflection::GenericValue& gv)
				{
					EcMesh* mesh = static_cast<EcMesh*>(p);
					mesh->SetRenderMesh(gv.AsStringView());
				}
			),

			REFLECTION_PROPERTY(
				"Material",
				String,
				[](void* p)
				-> Reflection::GenericValue
				{
					EcMesh* m = static_cast<EcMesh*>(p);
					MaterialManager* mtlManager = MaterialManager::Get();
				
					return mtlManager->GetMaterialResource(m->MaterialId).Name;
				},
				[](void* p, const Reflection::GenericValue& gv)
				{
					EcMesh* m = static_cast<EcMesh*>(p);
					MaterialManager* mtlManager = MaterialManager::Get();
				
					m->MaterialId = mtlManager->LoadFromPath(gv.AsStringView());
				}
			)
        };

        return props;
    }
};

static inline MeshManager Instance;

void EcMesh::SetRenderMesh(const std::string_view& MeshPath)
{
	if (MeshPath == Asset)
		return;

	if (RenderMeshId != UINT32_MAX)
		tryMarkFreeSkinnedMeshPseudoAsset(*this);

	MeshProvider* meshProvider = MeshProvider::Get();
	ObjectRef obj = Object;
	std::string meshPathStr = std::string(MeshPath);

	this->RenderMeshId = meshProvider->LoadFromPath(
		std::string(MeshPath),
		true,
		false,
		[obj, meshPathStr, meshProvider](Mesh& mesh)
		{
			if (mesh.Bones.size() == 0)
				return;

			// TODO
			// due to skinned mesh transforms being associated with ASSETS
			// and NOT the object using them, these need to be separate to
			// allow multiple instances of the same mesh to have different
			// transforms

			uint32_t meshId = UINT32_MAX;

			if (std::vector<uint32_t>& freelist = FreeSkinnedMeshPseudoAssets[meshPathStr];
				freelist.size() == 0
			)
			{
				std::string pseudoPath = meshPathStr + "!" + std::to_string(obj->ObjectId);
				meshId = meshProvider->Assign(mesh, pseudoPath);
			}
			else
			{
				meshId = freelist[0];
				freelist[0] = freelist[freelist.size() - 1];
				freelist.erase(freelist.begin() + freelist.size() - 1);
			}

			EcMesh* cm = obj->FindComponent<EcMesh>();
			cm->RenderMeshId = meshId;

			for (GameObject* ch : obj->GetChildren())
				if (ch->FindComponent<EcBone>())
					ch->Destroy();

			Mesh& meshAfter = meshProvider->GetMeshResource(meshId);

			for (uint8_t boneId = 0; boneId < meshAfter.Bones.size(); boneId++)
			{
				const Bone& b = meshAfter.Bones[boneId];
			
				ObjectRef boneObj;
				if (GameObject* g = obj->FindChild(b.Name))
				{
					if (g->FindComponent<EcBone>())
						boneObj = g;
					else
					{
						Log.WarningF(
							"Non-bone with name '{}' under Mesh at {} will be removed for rig bone",
							b.Name, obj->GetFullName()
						);

						g->Destroy();
						boneObj = GameObject::Create(EntityComponent::Bone);
					}
				}
				else
					boneObj = GameObject::Create(EntityComponent::Bone);
				
				EcBone* bone = boneObj->FindComponent<EcBone>();

				boneObj->SetParent(b.Parent == UINT8_MAX
					? obj.Referred()
					: obj->FindChild(meshAfter.Bones[b.Parent].Name)
				);
				boneObj->Name = b.Name;
				boneObj->Serializes = false;
				
				bone->Transform = b.Transform;
				bone->SkeletalBoneId = boneId;
				bone->TargetMesh = cm->ComponentId;
			}
		}
	);
	this->Asset = MeshPath;
}
