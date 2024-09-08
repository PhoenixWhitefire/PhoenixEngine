#include"gameobject/Workspace.hpp"

RegisterDerivedObject<Object_Workspace> Object_Workspace::RegisterClassAs("Workspace");
static bool s_DidInitReflection = false;

static auto scget(GameObject* g)
{
	return dynamic_cast<Object_Workspace*>(g)->GetSceneCamera()->Name;
}

static void scset(GameObject* g, Reflection::GenericValue gv)
{
	Object_Workspace* p = dynamic_cast<Object_Workspace*>(g);
	auto newObj = p->GetChild(gv.String);
	auto newCam = newObj ? dynamic_cast<Object_Camera*>(newObj) : nullptr;
	if (newCam)
		p->SetSceneCamera(newCam);
}

void Object_Workspace::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);
	REFLECTION_INHERITAPI(Object_Model);

	REFLECTION_DECLAREPROP(
		"SceneCamera",
		GameObject,
		scget,
		scset
	);
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
