#include"gameobject/Workspace.hpp"
#include"Debug.hpp"

DerivedObjectRegister<Object_Workspace> Object_Workspace::RegisterClassAs("Workspace");

Object_Workspace::Object_Workspace()
{
	this->Name = "Workspace";
	this->ClassName = "Workspace";

	std::shared_ptr<Object_Workspace> WorkspacePtr = std::make_shared<Object_Workspace>(*this);
	std::shared_ptr<GameObject> MeAsObject = std::dynamic_pointer_cast<GameObject>(WorkspacePtr);

	std::shared_ptr<Object_Camera> FallbackCamera = std::dynamic_pointer_cast<Object_Camera>
		(GameObjectFactory::CreateGameObject("Camera"));

	FallbackCamera->GenericMovement = true;
	FallbackCamera->IsSceneCamera = true;

	this->m_children.push_back(std::dynamic_pointer_cast<GameObject>(FallbackCamera));
	FallbackCamera->Parent = MeAsObject;

	this->m_sceneCamera = FallbackCamera;

	m_properties.insert(std::pair(
		"SceneCamera",
		PropInfo
		{
			PropType::String,
			PropReflection
			{
			[this]()
			{
				return m_sceneCamera->Name;
			},

			[this](GenericType gt)
			{
				auto newObj = this->GetChild(gt.String);
				auto newCam = newObj ? std::dynamic_pointer_cast<Object_Camera>(newObj) : nullptr;

				if (newCam)
				{
					this->m_sceneCamera->IsSceneCamera = false;
					this->m_sceneCamera = newCam;
					newCam->IsSceneCamera = true;
				}
				//else
					//Debug::Log(std::vformat(
					//	"Attempted to change the scene camera to '{}', but it is not a child of Workspace!",
					//	std::make_format_args(gt.String)
					//));
			}
		}
		}
	));
}

std::shared_ptr<Object_Camera> Object_Workspace::GetSceneCamera()
{
	return m_sceneCamera;
}

void Object_Workspace::SetSceneCamera(std::shared_ptr<Object_Camera> NewCam)
{
	this->m_sceneCamera = NewCam;
}
