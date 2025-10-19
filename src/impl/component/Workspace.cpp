#include <glm/gtc/matrix_transform.hpp>

#include "component/Workspace.hpp"
#include "component/Camera.hpp"
#include "component/Sound.hpp"
#include "Engine.hpp"

static ObjectHandle s_FallbackCamera;

static GameObject* createCamera()
{
	GameObject* camera = GameObject::Create("Camera");
	camera->FindComponent<EcCamera>()->UseSimpleController = true;

	return camera;
}

class WorkspaceManager : public ComponentManager<EcWorkspace>
{
public:
    uint32_t CreateComponent(GameObject* Object) override
    {
        m_Components.emplace_back();

		m_Components.back().Object = Object;
		m_Components.back().m_SceneCameraId = PHX_GAMEOBJECT_NULL_ID;

        return static_cast<uint32_t>(m_Components.size() - 1);
    }
	
	void Shutdown() override
	{
		s_FallbackCamera.Reference = nullptr; // Reference is not valid at this point
		ComponentManager<EcWorkspace>::Shutdown();
	}

    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = 
        {
			{
				"SceneCamera",
				{
					(Reflection::ValueType)((uint8_t)Reflection::ValueType::GameObject + (uint8_t)Reflection::ValueType::Null),
					[](void* p)
					-> Reflection::GenericValue
					{
						GameObject* cam = static_cast<EcWorkspace*>(p)->GetSceneCamera();

						if (cam->ObjectId != s_FallbackCamera->ObjectId)
							return cam->ToGenericValue();
						else
							return {}; // Null
					},
					[](void* p, const Reflection::GenericValue& gv)
					{
						static_cast<EcWorkspace*>(p)->SetSceneCamera(GameObject::FromGenericValue(gv));
					}
				}
			}
        };

        return props;
    }

    const Reflection::StaticMethodMap& GetMethods() override
    {
        static const Reflection::StaticMethodMap funcs =
		{
			{ "ScreenPointToRay", {
				{ Reflection::ValueType::Array, Reflection::ValueType::Double },
				{ Reflection::ValueType::Vector3 },
				[](void* p, const std::vector<Reflection::GenericValue>& inputs)
				-> std::vector<Reflection::GenericValue>
				{
					EcWorkspace* w = static_cast<EcWorkspace*>(p);

					std::span<Reflection::GenericValue> coordsgv = inputs[0].AsArray();
					
					double x = coordsgv[0].AsDouble();
					double y = coordsgv[1].AsDouble();

					float length = 1.f;

					if (inputs.size() > 1)
						length = static_cast<float>(inputs[1].AsDouble());

					return { w->ScreenPointToRay(x, y, length, nullptr) };
				}
			} }
		};
        return funcs;
    }
};

static inline WorkspaceManager Instance{};

glm::vec3 EcWorkspace::ScreenPointToRay(double x, double y, float length, glm::vec3* /* Origin */) const
{
	Engine* engine = Engine::Get();

	// thinmatrix 27/12/2024
	// https://www.youtube.com/watch?v=DLKN0jExRIM
	double nx = (2.f * x) / engine->WindowSizeX - 1;
	double ny = -((2.f * y) / engine->WindowSizeY - 1);

	glm::vec4 clipCoords{ nx, ny, -1.f, 1.f };

	EcCamera* cam = GetSceneCamera()->FindComponent<EcCamera>();

	glm::mat4 projectionMatrixInv = glm::inverse(glm::perspective(
		glm::radians(cam->FieldOfView),
		(float)engine->WindowSizeX / (float)engine->WindowSizeY,
		cam->NearPlane,
		cam->FarPlane
	));

	glm::vec4 eyeCoords = projectionMatrixInv * clipCoords;
	eyeCoords.z = -1.f, eyeCoords.w = 0.f;

	glm::vec3 position = glm::vec3(cam->Transform[3]);
	glm::vec3 forwardVec = glm::vec3(cam->Transform[2]);

	glm::mat4 viewMatrixInv = glm::inverse(glm::lookAt(
		position,
		position + forwardVec,
		glm::vec3(cam->Transform[1])
	));

	glm::vec3 rayVector = glm::normalize(glm::vec3(viewMatrixInv * eyeCoords)) * length;

	return rayVector;
}

GameObject* EcWorkspace::GetSceneCamera() const
{
	if (!s_FallbackCamera.HasValue())
		s_FallbackCamera = createCamera();

	GameObject* sceneCam = GameObject::GetObjectById(m_SceneCameraId);

	if (sceneCam && !sceneCam->FindComponent<EcCamera>())
	{
		Log::Warning("Scene Camera lost it's Camera component!");
		sceneCam = nullptr;
	}

	return sceneCam ? sceneCam : s_FallbackCamera.Dereference();
}

void EcWorkspace::SetSceneCamera(GameObject* NewCam)
{
	if (NewCam && !NewCam->FindComponent<EcCamera>())
		RAISE_RT("Must have a Camera component!");

	if (GameObject* prevCam = GetSceneCamera())
		if (prevCam != NewCam)
			prevCam->FindComponent<EcCamera>()->IsSceneCamera = false;

	m_SceneCameraId = NewCam ? NewCam->ObjectId : UINT32_MAX;

	if (NewCam)
		NewCam->FindComponent<EcCamera>()->IsSceneCamera = true;
}

void EcWorkspace::Update() const
{
	SoundManager& soundManager = SoundManager::Get();
	soundManager.Update(GetSceneCamera()->FindComponent<EcCamera>()->Transform);
}
