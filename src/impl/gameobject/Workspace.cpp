#include <glm/gtc/matrix_transform.hpp>

#include "gameobject/Workspace.hpp"
#include "Engine.hpp"

PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(Workspace);

static bool s_DidInitReflection = false;

static Object_Camera* s_FallbackCamera = nullptr;

static Object_Camera* createCamera()
{
	Object_Camera* camera = static_cast<Object_Camera*>(GameObject::Create("Camera"));
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
			GameObject* object = GameObject::FromGenericValue(gv);
			Object_Camera* newCam = object ? dynamic_cast<Object_Camera*>(object) : nullptr;

			if (!newCam && object)
				throw(std::string("SceneCamera must be a Camera!"));

			((Object_Workspace*)p)->SetSceneCamera(newCam);
		}
	);

	REFLECTION_DECLAREFUNC(
		"ScreenPointToRay",
		{ Reflection::ValueType::Array, Reflection::ValueType::Double },
		{ Reflection::ValueType::Map },
		[](Reflection::Reflectable* r, const std::vector<Reflection::GenericValue>& inputs)
		-> std::vector<Reflection::GenericValue>
		{
			Object_Workspace* p = static_cast<Object_Workspace*>(r);

			std::vector<Reflection::GenericValue> coordsgv = inputs.at(0).AsArray();

			double x = coordsgv.at(0).AsDouble();
			double y = coordsgv.at(1).AsDouble();

			float length = 1.f;

			if (inputs.size() > 2)
				length = static_cast<float>(inputs.at(2).AsDouble());

			Engine* engine = Engine::Get();
			int winSizeX = 0, winSizeY = 0;
			SDL_GetWindowSize(engine->Window, &winSizeX, &winSizeY);

			// thinmatrix 27/12/2024
			// https://www.youtube.com/watch?v=DLKN0jExRIM
			double nx = (2.f * x) / winSizeX - 1;
			double ny = -((2.f * y) / winSizeY - 1);

			glm::vec4 clipCoords{ nx, ny, -1.f, 1.f };

			Object_Camera* cam = p->GetSceneCamera();

			glm::mat4 projectionMatrixInv = glm::inverse(glm::perspective(
				glm::radians(cam->FieldOfView),
				(float)winSizeX / (float)winSizeY,
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

			return { Vector3(rayVector).ToGenericValue() };
		}
	);
}

Object_Workspace::Object_Workspace()
{
	this->Name = "Workspace";
	this->ClassName = "Workspace";

	s_DeclareReflections();
	ApiPointer = &s_Api;
}

void Object_Workspace::Initialize()
{
	Object_Camera* camera = createCamera();

	camera->SetParent(this);
	this->SetSceneCamera(camera);
}

Object_Camera* Object_Workspace::GetSceneCamera() const
{
	if (!s_FallbackCamera)
		s_FallbackCamera = createCamera();

	Object_Camera* sceneCam = dynamic_cast<Object_Camera*>(GameObject::GetObjectById(m_SceneCameraId));

	return sceneCam ? sceneCam : s_FallbackCamera;
}

void Object_Workspace::SetSceneCamera(Object_Camera* NewCam)
{
	if (Object_Camera* prevCam = dynamic_cast<Object_Camera*>(GameObject::GetObjectById(m_SceneCameraId)))
		if (prevCam != NewCam)
			prevCam->IsSceneCamera = false;

	m_SceneCameraId = NewCam ? NewCam->ObjectId : UINT32_MAX;

	if (NewCam)
		NewCam->IsSceneCamera = true;
}
