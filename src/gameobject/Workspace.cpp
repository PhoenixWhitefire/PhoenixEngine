#include"gameobject/Workspace.hpp"

RegisterDerivedObject<Object_Workspace> Object_Workspace::RegisterClassAs("Workspace");
bool Object_Workspace::s_DidInitReflection = false;

void Object_Workspace::s_DeclareReflections()
{
	if (s_DidInitReflection)
		//return;
	s_DidInitReflection = true;

	REFLECTION_DECLAREPROP(
		"SceneCamera",
		String,
		[](Reflection::BaseReflectable* g)
		{
			return dynamic_cast<Object_Workspace*>(g)->GetSceneCamera()->Name;
		},
		[](Reflection::BaseReflectable* g, Reflection::GenericValue gv)
		{
			Object_Workspace* p = dynamic_cast<Object_Workspace*>(g);

			auto newObj = p->GetChild(gv.String);
			auto newCam = newObj ? dynamic_cast<Object_Camera*>(newObj) : nullptr;

			if (newCam)
				p->SetSceneCamera(newCam);
			//else
				//Debug::Log(std::vformat(
				//	"Attempted to change the scene camera to '{}', but it is not a child of Workspace!",
				//	std::make_format_args(gv.String)
				//));
		}
	);

	REFLECTION_INHERITAPI(Object_Model);
}

Object_Workspace::Object_Workspace()
{
	this->Name = "Workspace";
	this->ClassName = "Workspace";
	this->m_sceneCamera = nullptr;

	s_DeclareReflections();
}

void Object_Workspace::Initialize()
{
	GameObject* workspaceBase = dynamic_cast<GameObject*>(this);

	GameObject* fallbackCameraBase = GameObject::CreateGameObject("Camera");
	Object_Camera* fallbackCamera = dynamic_cast<Object_Camera*>(fallbackCameraBase);

	fallbackCamera->GenericMovement = true;
	fallbackCamera->IsSceneCamera = true;

	fallbackCameraBase->SetParent(this);

	this->m_sceneCamera = fallbackCamera;
}

Object_Camera* Object_Workspace::GetSceneCamera()
{
	return m_sceneCamera;
}

void Object_Workspace::SetSceneCamera(Object_Camera* NewCam)
{
	if (m_sceneCamera && m_sceneCamera != NewCam)
		m_sceneCamera->IsSceneCamera = false;

	this->m_sceneCamera = NewCam;
	NewCam->IsSceneCamera = true;
}
