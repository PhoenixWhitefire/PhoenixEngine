// The Hierarchy Root
// 13/08/2024

#include"gameobject/DataModel.hpp"
#include"Debug.hpp"

RegisterDerivedObject<Object_DataModel> Object_DataModel::RegisterObjectAs("DataModel");
static bool s_DidInitReflection = false;

static Reflection::GenericValue closeDataModel(GameObject* g, const Reflection::GenericValue&)
{
	dynamic_cast<Object_DataModel*>(g)->WantExit = true;
	return Reflection::GenericValue();
}

void Object_DataModel::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);

	REFLECTION_DECLAREPROC("Close", closeDataModel);
	REFLECTION_DECLAREPROC(
		"Destroy",
		[](GameObject*, const Reflection::GenericValue&) -> Reflection::GenericValue
		{
			throw("Cannot Destroy a DataModel");
		}
	);
}

Object_DataModel::Object_DataModel()
{
	this->Name = "DataModel";
	this->ClassName = "DataModel";

	s_DeclareReflections();
}

Object_DataModel::~Object_DataModel()
{
	// TODO: :BindToClose 15/08/2024
	Debug::Log("DataModel destructing...");
}

void Object_DataModel::Initialize()
{
	auto workspace = GameObject::CreateGameObject("Workspace");
	workspace->SetParent(this);

	Debug::Log("DataModel initialized");
}
