#include "gameobject/Workspace.hpp"

RegisterDerivedObject<Object_Workspace> Object_Workspace::RegisterClassAs("Workspace");
static bool s_DidInitReflection = false;

static Object_Camera* s_FallbackCamera = nullptr;

static Object_Camera* createCamera()
{
	Object_Camera* camera = dynamic_cast<Object_Camera*>(GameObject::Create("Camera"));
	camera->UseSimpleController = true;

	return camera;
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
		[](GameObject* p)
		{
			Object_Workspace* workspace = dynamic_cast<Object_Workspace*>(p);

			if (workspace->GetSceneCamera() == s_FallbackCamera)
				return ((GameObject*)nullptr)->ToGenericValue();
			else
				return workspace->GetSceneCamera()->ToGenericValue();
		},
		[](GameObject* p, const Reflection::GenericValue& gv)
		{
			dynamic_cast<Object_Workspace*>(p)->SetSceneCamera(
				dynamic_cast<Object_Camera*>(GameObject::FromGenericValue(gv))
			);
		}
	);
}

Object_Workspace::Object_Workspace()
{
	this->Name = "Workspace";
	this->ClassName = "Workspace";
	m_SceneCamera = nullptr;

	s_DeclareReflections();
}

void Object_Workspace::Initialize()
{
	Object_Camera* camera = createCamera();

	camera->SetParent(this);
	this->SetSceneCamera(camera);
}

Object_Camera* Object_Workspace::GetSceneCamera()
{
	if (!s_FallbackCamera)
		s_FallbackCamera = createCamera();

	return m_SceneCamera ? m_SceneCamera : s_FallbackCamera;
}

void Object_Workspace::SetSceneCamera(Object_Camera* NewCam)
{
	if (m_SceneCamera && m_SceneCamera != NewCam)
		m_SceneCamera->IsSceneCamera = false;

	m_SceneCamera = NewCam;

	if (NewCam)
		NewCam->IsSceneCamera = true;
}
