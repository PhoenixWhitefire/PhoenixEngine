#include <tracy/Tracy.hpp>

#include "component/Mesh.hpp"
#include "component/Transform.hpp"

#include "asset/MaterialManager.hpp"
#include "asset/MeshProvider.hpp"
#include "component/Workspace.hpp"
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

static float roundNToGrid(float x)
{
	return glm::round(x / SPATIAL_HASH_GRID_SIZE) * SPATIAL_HASH_GRID_SIZE;
}

static glm::vec3 roundToGrid(const glm::vec3& v)
{
	return glm::vec3(roundNToGrid(v.x), roundNToGrid(v.y), roundNToGrid(v.z));
}

static void updateSpatialHash(EcMesh* cm)
{
	if (GameObject* pw = cm->PrevWorkspace.Referred(); cm->SpatialHashPoints.size() > 0 && pw)
		if (EcWorkspace* pcw = pw->FindComponent<EcWorkspace>())
		{
			for (const glm::vec3& point : cm->SpatialHashPoints)
			{
				auto& cell = pcw->SpatialHash[point];
				auto it = std::find(cell.begin(), cell.end(), cm->Object->ObjectId);
				assert(it != cell.end());

				cell.erase(it);
			}
		}

	cm->SpatialHashPoints.clear();

	if (!cm->PhysicsCollisions)
		return;

	GameObject* ocw = GameObject::GetObjectById(cm->Object->OwningWorkspace);
	EcWorkspace* cw = ocw ? ocw->FindComponent<EcWorkspace>() : nullptr;
	cm->PrevWorkspace = ocw;

	if (!cw)
		return;

	glm::vec3 min = cm->CollisionAabb.Position - cm->CollisionAabb.Size / 2.f;
	glm::vec3 max = cm->CollisionAabb.Position + cm->CollisionAabb.Size / 2.f;
	
	min = roundToGrid(min);
	max = roundToGrid(max);

	if (min == max)
	{
		cw->SpatialHash[min].push_back(cm->Object.TargetId);
		cm->SpatialHashPoints = { min };
		return;
	}

	for (float x = min.x; x <= max.x; x += SPATIAL_HASH_GRID_SIZE)
		for (float y = min.y; y <= max.y; y += SPATIAL_HASH_GRID_SIZE)
			for (float z = min.z; z <= max.z; z += SPATIAL_HASH_GRID_SIZE)
			{
				glm::vec3 point = { x, y, z };
				assert(roundToGrid(point) == point);

				cw->SpatialHash[point].push_back(cm->Object.TargetId);
				cm->SpatialHashPoints.push_back(point);
			}
}

class MeshManager : public ComponentManager<EcMesh>
{
public:
    uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();

		if (!Object->FindComponent<EcTransform>())
			Object->AddComponent(EntityComponent::Transform);
		
		m_Components.back().MaterialId = MaterialManager::Get()->LoadFromPath("plastic");
		m_Components.back().Object = Object;
		
        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    void DeleteComponent(uint32_t Id) override
    {
        // TODO id reuse with handles that have a counter per re-use to reduce memory growth

		EcMesh& mesh = m_Components[Id];
		tryMarkFreeSkinnedMeshPseudoAsset(mesh);

		ComponentManager<EcMesh>::DeleteComponent(Id);
    }

    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = 
        {
            EC_PROP_SIMPLE(EcMesh, PhysicsDynamics, Boolean),
			EC_PROP_SIMPLE(EcMesh, PhysicsCollisions, Boolean),
			EC_PROP_SIMPLE(EcMesh, CastsShadows, Boolean),

			EC_PROP(
				"PhysicsCollisions",
				Boolean,
				EC_GET_SIMPLE(EcMesh, PhysicsCollisions),
				[](void* p, const Reflection::GenericValue& gv)
				{
					EcMesh* cm = static_cast<EcMesh*>(p);
					cm->PhysicsCollisions = gv.AsBoolean();
					updateSpatialHash(cm);
				}
			),

			EC_PROP(
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

			EC_PROP(
				"CollisionFidelity",
				Integer,
				[](void* p)
				-> Reflection::GenericValue
				{
					return static_cast<uint32_t>(static_cast<EcMesh*>(p)->CollisionFidelity);
				},
				[](void* p, const Reflection::GenericValue& gv)
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

			EC_PROP_SIMPLE(EcMesh, Friction, Double),

			EC_PROP(
				"Density",
				Double,
				EC_GET_SIMPLE(EcMesh, Density),
				[](void* p, const Reflection::GenericValue& gv)
				{
					EcMesh* cm = static_cast<EcMesh*>(p);
					float dens = (float)gv.AsDouble();

					if (dens < 0.001f)
						RAISE_RT("Minimum density is 0.001");

					cm->Density = dens;
					cm->Mass = dens * cm->CollisionAabb.Size.x * cm->CollisionAabb.Size.y * cm->CollisionAabb.Size.z;
				}
			),

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
	std::string meshPathStr{ MeshPath };

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

			obj->FindComponent<EcMesh>()->RenderMeshId = meshId;
			
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
						Log::WarningF(
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
			}
		}
	);
	this->Asset = MeshPath;
}

void EcMesh::RecomputeAabb()
{
	ZoneScoped;

	EcTransform* ct = this->Object->FindComponent<EcTransform>();
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

		this->CollisionAabb.Position = (min + max) / 2.f;
		this->CollisionAabb.Size = (max - min) / 2.f;
	}
	else if (CollisionFidelity == CollisionFidelityMode::AabbStaticSize)
	{
		this->CollisionAabb.Position = glm::vec3(transform[3]);
		this->CollisionAabb.Size = glmsize;
	}

	updateSpatialHash(this);

	this->Mass = Density * CollisionAabb.Size.x * CollisionAabb.Size.y * CollisionAabb.Size.z;
}

