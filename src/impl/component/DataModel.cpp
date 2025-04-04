// The Hierarchy Root
// 13/08/2024

#include "component/DataModel.hpp"
#include "Log.hpp"

class DataModelManager : BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) final
    {
        m_Components.emplace_back();

        /*
        // WorldArray might get re-alloc'd when we create the Workspace,
        // save our ID
        uint32_t objectId = Object->ObjectId;

		GameObject* workspace = GameObject::Create(EntityComponent::Workspace);
		workspace->SetParent(GameObject::GetObjectById(objectId));
		m_Components.back().Workspace = workspace->ObjectId;
        */

        return static_cast<uint32_t>(m_Components.size() - 1);
    }
    
    virtual std::vector<void*> GetComponents() final
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (const EcDataModel& t : m_Components)
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
        static const Reflection::PropertyMap props = {};

        return props;
    }

    virtual const Reflection::FunctionMap& GetFunctions() final
    {
        static const Reflection::FunctionMap funcs =
		{
			{ "Close",
				{ 
					{},
					{},
					[](void* p, const std::vector<Reflection::GenericValue>&)
                    -> std::vector<Reflection::GenericValue>
					{
						static_cast<EcDataModel*>(p)->WantExit = true;
                        return {};
					}
				}
			}
		};
        return funcs;
    }

    DataModelManager()
    {
        GameObject::s_ComponentManagers[(size_t)EntityComponent::DataModel] = this;
    }

private:
    std::vector<EcDataModel> m_Components;
};

static inline DataModelManager Instance{};

/*

PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(DataModel);

static bool s_DidInitReflection = false;

static void closeDataModel(Reflection::Reflectable* g)
{
	static_cast<Object_DataModel*>(g)->WantExit = true;
}

void Object_DataModel::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);

	REFLECTION_DECLAREPROC_INPUTLESS(Close, closeDataModel);
}

Object_DataModel::Object_DataModel()
{
	this->Name = "DataModel";
	this->ClassName = "DataModel";

	s_DeclareReflections();
	ApiPointer = &s_Api;
}

Object_DataModel::~Object_DataModel()
{
	// TODO: :BindToClose 15/08/2024
}

void Object_DataModel::Initialize()
{
	GameObject* workspace = GameObject::Create("Workspace");
	workspace->SetParent(this);
}

Object_Workspace* Object_DataModel::GetWorkspace()
{
	return dynamic_cast<Object_Workspace*>(this->FindChild("Workspace"));
}
*/
