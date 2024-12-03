// The Hierarchy Root
// 13/08/2024

#include "gameobject/DataModel.hpp"
#include "Debug.hpp"

PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(DataModel);

static bool s_DidInitReflection = false;

static void closeDataModel(Reflection::Reflectable* g)
{
	dynamic_cast<Object_DataModel*>(g)->WantExit = true;
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
