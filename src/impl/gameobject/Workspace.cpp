#include"gameobject/Workspace.hpp"

RegisterDerivedObject<Object_Workspace> Object_Workspace::RegisterClassAs("Workspace");
static bool s_DidInitReflection = false;

static Object_Camera* s_FallbackCamera = nullptr;

static void createDefaultCamera()
{
	s_FallbackCamera = dynamic_cast<Object_Camera*>(GameObject::CreateGameObject("Camera"));
	s_FallbackCamera->GenericMovement = true;
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

			Reflection::GenericValue gv;
			gv.Type = Reflection::ValueType::GameObject;

			gv.Integer = workspace->GetSceneCamera()
						? workspace->GetSceneCamera()->ObjectId
						: PHX_GAMEOBJECT_NULL_ID;

			return gv;
		},
		[](GameObject* p, const Reflection::GenericValue& gv)
		{
			dynamic_cast<Object_Workspace*>(p)->SetSceneCamera(
				dynamic_cast<Object_Camera*>(GameObject::GetObjectById(static_cast<uint32_t>(gv.Integer)))
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
	if (!s_FallbackCamera)
	{
		createDefaultCamera();
		
		s_FallbackCamera->SetParent(this);
		this->SetSceneCamera(s_FallbackCamera);
	}
}

Object_Camera* Object_Workspace::GetSceneCamera()
{
	if (!s_FallbackCamera)
		createDefaultCamera();

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
