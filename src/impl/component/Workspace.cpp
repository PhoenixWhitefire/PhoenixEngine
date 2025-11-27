#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <tracy/public/tracy/Tracy.hpp>

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
			{ "SceneCamera", {
				REFLECTION_OPTIONAL(GameObject),
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
			} }
        };

        return props;
    }

    const Reflection::StaticMethodMap& GetMethods() override
    {
        static const Reflection::StaticMethodMap funcs =
		{
			{ "ScreenPointToVector", {
				{ Reflection::ValueType::Vector2, REFLECTION_OPTIONAL(Double) },
				{ Reflection::ValueType::Vector3 },
				[](void* p, const std::vector<Reflection::GenericValue>& inputs)
				-> std::vector<Reflection::GenericValue>
				{
					EcWorkspace* w = static_cast<EcWorkspace*>(p);

					glm::vec2 point = inputs[0].AsVector2();
					float length = 1.f;

					if (inputs.size() > 1)
						length = (float)inputs[1].AsDouble();

					return { w->ScreenPointToVector(point, length) };
				}
			} },

			{ "Raycast", {
				{ Reflection::ValueType::Vector3, Reflection::ValueType::Vector3, REFLECTION_OPTIONAL(Array), REFLECTION_OPTIONAL(Boolean) },
				{ REFLECTION_OPTIONAL(Map) },
				[](void* p, const std::vector<Reflection::GenericValue>& inputs)
				-> std::vector<Reflection::GenericValue>
				{
					const glm::vec3& origin = inputs[0].AsVector3();
					const glm::vec3& vector = inputs[1].AsVector3();

					std::vector<GameObject*> filterList;

					if (inputs.size() > 2)
					{
						const std::span<Reflection::GenericValue>& filterListGv = inputs[2].AsArray();
						filterList.reserve(filterListGv.size());

						for (const Reflection::GenericValue& gv : filterListGv)
							filterList.push_back(GameObject::FromGenericValue(gv));
					}

					SpatialCastResult result = static_cast<EcWorkspace*>(p)->Raycast(origin, vector, filterList, inputs.size() > 3 ? inputs[3].AsBoolean() : true);
					
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
				{ Reflection::ValueType::Vector3, Reflection::ValueType::Vector3, REFLECTION_OPTIONAL(Array) },
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

glm::vec3 EcWorkspace::ScreenPointToVector(glm::vec2 point, float length) const
{
	Engine* engine = Engine::Get();

	float x = point.x;
	float y = point.y;
	x = x - engine->ViewportPosition.x;
	y = y - engine->ViewportPosition.y;

	ImVec2 viewportSize = engine->GetViewportSize();

	// thinmatrix 27/12/2024
	// https://www.youtube.com/watch?v=DLKN0jExRIM
	double nx = (2.0 * x) / viewportSize.x - 1.0;
	double ny = 1.0 - (2.0 * y) / viewportSize.y;

	glm::vec4 clipCoords = { nx, ny, -1.f, 1.f };

	EcCamera* cam = GetSceneCamera()->FindComponent<EcCamera>();
	const glm::mat4& trans = cam->GetWorldTransform();

	glm::mat4 projectionMatrixInv = glm::inverse(glm::perspective(
		glm::radians(cam->FieldOfView),
		viewportSize.x / viewportSize.y,
		cam->NearPlane,
		cam->FarPlane
	));

	glm::vec4 eyeCoords = projectionMatrixInv * clipCoords;
	eyeCoords.z = -1.f, eyeCoords.w = 0.f;

	glm::vec3 position = glm::vec3(trans[3]);
	glm::vec3 forwardVec = glm::vec3(trans[2]);

	glm::mat4 viewMatrixInv = glm::inverse(glm::lookAt(
		position,
		position + forwardVec,
		glm::vec3(trans[1])
	));

	return glm::normalize(glm::vec3(viewMatrixInv * eyeCoords)) * length;
}

static int roundNToGrid(float x)
{
	return int(glm::round(x / SPATIAL_HASH_GRID_SIZE) * SPATIAL_HASH_GRID_SIZE);
}

static glm::ivec3 roundToGrid(const glm::vec3& v)
{
	return glm::ivec3(roundNToGrid(v.x), roundNToGrid(v.y), roundNToGrid(v.z));
}

static void hashTraceRay(
	const EcWorkspace* cw,
	const glm::vec3& rayStart,
	const glm::ivec3& rayStartCell,
	const glm::ivec3& rayEndCell,
	const glm::vec3& rayVector,
	const std::function<bool(const std::vector<uint32_t>&)> TestHits
)
{
	ZoneScoped;

	glm::ivec3 step = glm::sign(rayVector) * SPATIAL_HASH_GRID_SIZE;
	glm::vec3 rayDir = glm::normalize(rayVector);

	glm::vec3 tDelta = glm::vec3(
		rayDir.x != 0.f ? std::abs(1.f / rayDir.x) * SPATIAL_HASH_GRID_SIZE : FLT_MAX,
		rayDir.y != 0.f ? std::abs(1.f / rayDir.y) * SPATIAL_HASH_GRID_SIZE : FLT_MAX,
		rayDir.z != 0.f ? std::abs(1.f / rayDir.z) * SPATIAL_HASH_GRID_SIZE : FLT_MAX
	);

	glm::vec3 tmax;

	for (int i = 0; i < 3; i++)
	{
		if (step[i] > 0)
			tmax[i] = ((rayStartCell[i] + 1) - rayStart[i]) / rayDir[i];
		else if (step[i] < 0)
			tmax[i] = (rayStartCell[i] - rayStart[i]) / rayDir[i];
		else
			tmax[i] = FLT_MAX;
	}

	glm::ivec3 currentCell = rayStartCell;
	int depth = 0;

	while (true)
	{
		const auto& cellIt = cw->SpatialHash.find(currentCell);

		if (cellIt != cw->SpatialHash.end() && TestHits(cellIt->second))
			return; // hit something

		if (currentCell == rayEndCell)
			return;

		if (tmax.x < tmax.y)
        {
			if (tmax.x < tmax.z)
			{
                currentCell.x += step.x;
                tmax.x += tDelta.x;
            }
			else
			{
                currentCell.z += step.z;
                tmax.z += tDelta.z;
            }
        }
        else
        {
            if (tmax.y < tmax.z)
			{
                currentCell.y += step.y;
                tmax.y += tDelta.y;
            }
			else
			{
                currentCell.z += step.z;
                tmax.z += tDelta.z;
            }
        }

		if (++depth > 1024)
			return;
	}
}

SpatialCastResult EcWorkspace::Raycast(const glm::vec3& Origin, const glm::vec3& Vector, const std::vector<GameObject*>& FilterList, bool FilterIsIgnoreList) const
{
	ZoneScoped;

	IntersectionLib::Intersection intersection;
	GameObject* hitObject = nullptr;
	float closestHit = FLT_MAX;

	std::vector<GameObject*> objects;
	if (FilterIsIgnoreList)
		objects = Object->GetDescendants();
	else
	{
		objects = FilterList;
		for (GameObject* p : FilterList)
		{
			std::vector<GameObject*> descs = p->GetDescendants();
			objects.insert(objects.end(), descs.begin(), descs.end());
		}
	}

	hashTraceRay(this, Origin, roundToGrid(Origin), roundToGrid(Origin + Vector), Vector, [&](const std::vector<uint32_t>& CellObjects) -> bool
	{
		ZoneScopedN("VisitCell");

		for (uint32_t oid : CellObjects)
		{
			GameObject* p = GameObject::GetObjectById(oid);
			if (!p)
				continue;

			const auto& filterIt = std::find(FilterList.begin(), FilterList.end(), p);

			if (FilterIsIgnoreList ? filterIt != FilterList.end() : filterIt == FilterList.end())
				continue;

			EcMesh* cm = p->FindComponent<EcMesh>();
			EcTransform* ct = p->FindComponent<EcTransform>();

			if (cm && ct)
			{
				IntersectionLib::Intersection hit = IntersectionLib::RayAabb(
					Origin,
					Vector,
					cm->CollisionAabb.Position,
					cm->CollisionAabb.Size - glm::vec3(2.f)
				);

				if (hit.Occurred)
					if (hit.Time < closestHit)
					{
						intersection = hit;
						closestHit = hit.Time;
						hitObject = p;
					}
			}
		}

		if (hitObject)
			return true; // hit something, no need to keep traversing the hash
		else
			return false; // keep checking for collisions further down our hash
	});

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
	soundManager.Update(GetSceneCamera()->FindComponent<EcCamera>()->GetWorldTransform());
}
