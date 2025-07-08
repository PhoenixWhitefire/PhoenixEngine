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

    virtual void DeleteComponent(uint32_t) override
    {
        // TODO id reuse with handles that have a counter per re-use to reduce memory growth
    }

    virtual const Reflection::PropertyMap& GetProperties() override
    {
        static const Reflection::PropertyMap props =
		{

			EC_PROP(
				"LocalTransform",
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
						realBone->Transform = gv.AsMatrix();
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

    virtual const Reflection::FunctionMap& GetFunctions() override
    {
        static const Reflection::FunctionMap funcs =
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
