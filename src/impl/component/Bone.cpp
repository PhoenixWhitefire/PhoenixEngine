#include "component/Bone.hpp"
#include "component/Mesh.hpp"
#include "datatype/GameObject.hpp"
#include "asset/MeshProvider.hpp"

static Bone* getUnderlyingBone(EcBone* BoneComponent)
{
	GameObject* meshObj = BoneComponent->Object->GetParent();
	EcMesh* mesh = meshObj ? meshObj->GetComponent<EcMesh>() : nullptr;

	if (mesh)
	{
		Mesh& meshData = MeshProvider::Get()->GetMeshResource(mesh->RenderMeshId);
		std::vector<Bone>& bones = meshData.Bones;

		if (bones.size() >= (size_t)BoneComponent->SkeletalBoneId)
			return &bones.at(BoneComponent->SkeletalBoneId);
		else
			return nullptr;
	}
	else
		return nullptr;
}

static float lerp(float a, float b, float t)
{
	return a + (b - a) * t;
}

static glm::mat4 lerpMatrix(const glm::mat4& a, const glm::mat4& b, float t)
{
	glm::mat4 result;
	for (int i = 0; i < 4; ++i)
		for (int j = 0; j < 4; ++j)
			result[i][j] = lerp(a[i][j], b[i][j], t);
	
	return result;
}

class BoneManager : public BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();
		m_Components.back().Object = Object;

        return static_cast<uint32_t>(m_Components.size() - 1);
    }
    
    virtual std::vector<void*> GetComponents() override
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (EcBone& t : m_Components)
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
		m_Components[Id].Object.~GameObjectRef();
    }

	virtual void Shutdown() override
	{
		m_Components.clear();
	}

    virtual const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props =
		{

			EC_PROP(
				"Transform",
				Matrix,
				[](void* p)
				{
					EcBone* boneObj = static_cast<EcBone*>(p);
					Bone* realBone = getUnderlyingBone(boneObj);
					
					if (realBone)
						return realBone->Transform;
					else
						return boneObj->Transform;
				},
				[](void* p, const Reflection::GenericValue& gv)
				{
					EcBone* boneObj = static_cast<EcBone*>(p);
					Bone* realBone = getUnderlyingBone(boneObj);
					
					if (realBone)
					{
						realBone->Transform = gv.AsMatrix();

						GameObject* meshObj = boneObj->Object->GetParent();
						EcMesh* mesh = meshObj->GetComponent<EcMesh>();

						Mesh& meshData = MeshProvider::Get()->GetMeshResource(mesh->RenderMeshId);
						MeshProvider::GpuMesh& gpuMesh = MeshProvider::Get()->GetGpuMesh(meshData.GpuId);

						for (auto [vi, ji] : realBone->TargetVertices)
						{
							const Vertex& v = meshData.Vertices[vi];
							gpuMesh.SkinningData[vi] = lerpMatrix(glm::mat4(1.f), realBone->Transform, v.JointWeights[ji]);
						}
					}
					else
						boneObj->Transform = gv.AsMatrix();
				}
			),

			EC_PROP(
				"IsActive",
				Boolean,
				[](void* p)
				{
					return getUnderlyingBone(static_cast<EcBone*>(p)) != nullptr;	
				},
				nullptr
			),

			EC_PROP(
				"SkeletalBoneId",
				Integer,
				[](void* p)
				{
					return (uint32_t)static_cast<EcBone*>(p)->SkeletalBoneId;
				},
				nullptr
			)

		};

        return props;
    }

    virtual const Reflection::StaticMethodMap& GetMethods() override
    {
        static const Reflection::StaticMethodMap funcs =
		{
			
		};
        return funcs;
    }

    BoneManager()
    {
        GameObject::s_ComponentManagers[(size_t)EntityComponent::Bone] = this;
    }

private:
    std::vector<EcBone> m_Components;
};

static BoneManager Instance{};
