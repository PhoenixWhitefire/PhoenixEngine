#include "gameobject/Workspace.hpp"

PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(Workspace);

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

	REFLECTION_INHERITAPI(Model);

	REFLECTION_DECLAREPROP(
		"SceneCamera",
		GameObject,
		[](Reflection::Reflectable* p)
		{
			Object_Workspace* workspace = (Object_Workspace*)p;

			if (workspace->GetSceneCamera() == s_FallbackCamera)
				return ((GameObject*)nullptr)->ToGenericValue();
			else
				return workspace->GetSceneCamera()->ToGenericValue();
		},
		[](Reflection::Reflectable* p, const Reflection::GenericValue& gv)
		{
			((Object_Workspace*)p)->SetSceneCamera(
				(Object_Camera*)GameObject::FromGenericValue(gv)
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
	ApiPointer = &s_Api;
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
