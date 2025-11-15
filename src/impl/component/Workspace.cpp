#include <glm/gtc/matrix_transform.hpp>

#include "component/Workspace.hpp"
#include "component/Transform.hpp"
#include "component/Camera.hpp"
#include "component/Sound.hpp"
#include "IntersectionLib.hpp"
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
					REFLECTION_OPTIONAL(Reflection::ValueType::GameObject),
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
				{ Reflection::ValueType::Double, Reflection::ValueType::Double, REFLECTION_OPTIONAL(Reflection::ValueType::Double) },
				{ Reflection::ValueType::Vector3 },
				[](void* p, const std::vector<Reflection::GenericValue>& inputs)
				-> std::vector<Reflection::GenericValue>
				{
					EcWorkspace* w = static_cast<EcWorkspace*>(p);
					
					double x = inputs[0].AsDouble();
					double y = inputs[1].AsDouble();

					float length = 1.f;

					if (inputs.size() > 2)
						length = static_cast<float>(inputs[2].AsDouble());

					return { w->ScreenPointToRay(x, y, length, nullptr) };
				}
			} },

			{ "Raycast", {
				{ Reflection::ValueType::Vector3, Reflection::ValueType::Vector3, REFLECTION_OPTIONAL(Reflection::ValueType::Array) },
				{ REFLECTION_OPTIONAL(Reflection::ValueType::Map) },
				[](void* p, const std::vector<Reflection::GenericValue>& inputs)
				-> std::vector<Reflection::GenericValue>
				{
					const glm::vec3& origin = inputs[0].AsVector3();
					const glm::vec3& vector = inputs[1].AsVector3();

					std::vector<GameObject*> ignoreList;

					if (inputs.size() > 2)
					{
						const std::span<Reflection::GenericValue>& ignorelistgv = inputs[2].AsArray();
						ignoreList.reserve(ignorelistgv.size());

						for (const Reflection::GenericValue& gv : ignorelistgv)
							ignoreList.push_back(GameObject::FromGenericValue(gv));
					}

					SpatialCastResult result = static_cast<EcWorkspace*>(p)->Raycast(origin, vector, ignoreList);
					
					if (!result.Occurred)
						return { Reflection::GenericValue() };

					else
					{
						// TODO make datatypes easier to create
						// this should definitely be a `SpatialCastResult` datatype
						std::vector<Reflection::GenericValue> vals;
						vals.reserve(6);

						vals.emplace_back("Object");
						vals.emplace_back(result.Object->ToGenericValue());

						vals.emplace_back("Position");
						vals.emplace_back(result.Position);

						vals.emplace_back("Normal");
						vals.emplace_back(result.Normal);

						Reflection::GenericValue gv(vals);
						gv.Type = Reflection::ValueType::Map;

						return { gv };
					}
				}
			} },

			{ "GetObjectsInAabb", {
				{ Reflection::ValueType::Vector3, Reflection::ValueType::Vector3, REFLECTION_OPTIONAL(Reflection::ValueType::Array) },
				{ Reflection::ValueType::Array },
				[](void* p, const std::vector<Reflection::GenericValue>& inputs)
				-> std::vector<Reflection::GenericValue>
				{
					const glm::vec3& position = inputs[0].AsVector3();
					const glm::vec3& origin = inputs[1].AsVector3();

					std::vector<GameObject*> ignoreList;

					if (inputs.size() > 2)
					{
						const std::span<Reflection::GenericValue>& ignorelistgv = inputs[2].AsArray();
						ignoreList.reserve(ignorelistgv.size());

						for (const Reflection::GenericValue& gv : ignorelistgv)
							ignoreList.push_back(GameObject::FromGenericValue(gv));
					}

					std::vector<GameObject*> objects = static_cast<EcWorkspace*>(p)->GetObjectsInAabb(position, origin, ignoreList);

					std::vector<Reflection::GenericValue> gv(objects.size());
					for (size_t i = 0; i < objects.size(); i++)
						gv[i] = objects[i]->ToGenericValue();

					return { Reflection::GenericValue(gv) };
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

	x = x - engine->ViewportPosition.x;
	y = y - engine->ViewportPosition.y;

	ImVec2 viewportSize = engine->GetViewportSize();

	// thinmatrix 27/12/2024
	// https://www.youtube.com/watch?v=DLKN0jExRIM
	double nx = (2.0 * x) / viewportSize.x - 1.0;
	double ny = 1.0 - (2.0 * y) / viewportSize.y;

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

SpatialCastResult EcWorkspace::Raycast(const glm::vec3& Origin, const glm::vec3& Vector, const std::vector<GameObject*>& IgnoreList) const
{
	IntersectionLib::Intersection intersection;
	GameObject* hitObject = nullptr;
	float closestHit = FLT_MAX;

	for (GameObject* p : Object->GetDescendants())
	{
		if (std::find(IgnoreList.begin(), IgnoreList.end(), p) != IgnoreList.end())
			continue;
    
		EcMesh* object = p->FindComponent<EcMesh>();
		EcTransform* ct = p->FindComponent<EcTransform>();
    
		if (object && ct)
		{
			glm::vec3 pos = ct->Transform[3];
			glm::vec3 size = ct->Size;
        
			IntersectionLib::Intersection hit = IntersectionLib::LineAabb(
				Origin,
				Vector,
				pos,
				size
			);
        
			if (hit.Occurred)
				if (hit.Time < closestHit)
				{
					intersection = hit;
					closestHit = hit.Depth;
					hitObject = p;
				}
		}
	}

	SpatialCastResult result;

	if (hitObject)
	{
		result.Occurred = true;
		result.Object = hitObject;
		result.Position = intersection.Position;
		result.Normal = intersection.Normal;
	}

	return result;
}

std::vector<GameObject*> EcWorkspace::GetObjectsInAabb(const glm::vec3& Position, const glm::vec3& Size, const std::vector<GameObject*>& IgnoreList) const
{
	IntersectionLib::Intersection intersection;
	std::vector<GameObject*> hits;

	for (GameObject* p : Object->GetDescendants())
	{
		if (std::find(IgnoreList.begin(), IgnoreList.end(), p) != IgnoreList.end())
			continue;
    
		EcTransform* object = p->FindComponent<EcTransform>();
    
		if (object)
		{
			glm::vec3 bpos = object->Transform[3];
			glm::vec3 bsize = object->Size;
        
			IntersectionLib::Intersection hit = IntersectionLib::AabbAabb(
				Position,
				Size,
				bpos,
				bsize
			);
        
			if (hit.Occurred)
				hits.push_back(p);
		}
	}

	return hits;
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
