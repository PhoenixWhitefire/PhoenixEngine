// RigidBody.cpp, 22/01/2026

#include <tracy/Tracy.hpp>
#include <cfloat>

#include "component/RigidBody.hpp"
#include "component/Workspace.hpp"

static float roundNToGrid(float x)
{
	return glm::round(x / SPATIAL_HASH_GRID_SIZE) * SPATIAL_HASH_GRID_SIZE;
}

static glm::vec3 roundToGrid(const glm::vec3& v)
{
	return glm::vec3(roundNToGrid(v.x), roundNToGrid(v.y), roundNToGrid(v.z));
}

static void updateSpatialHash(EcRigidBody* crb)
{
	ZoneScoped;

	if (GameObject* pw = crb->PrevWorkspace.Referred(); crb->SpatialHashPoints.size() > 0 && pw)
		if (EcWorkspace* pcw = pw->FindComponent<EcWorkspace>())
		{
			for (const glm::vec3& point : crb->SpatialHashPoints)
			{
				auto& cell = pcw->SpatialHash[point];
				auto it = std::find(cell.begin(), cell.end(), crb->Object->ObjectId);
				assert(it != cell.end());

				cell.erase(it);
			}
		}

	crb->SpatialHashPoints.clear();

	if (!crb->PhysicsCollisions)
		return;

	GameObject* ocw = GameObject::GetObjectById(crb->Object->OwningWorkspace);
	EcWorkspace* cw = ocw ? ocw->FindComponent<EcWorkspace>() : nullptr;
	crb->PrevWorkspace = ocw;

	if (!cw)
		return;

	glm::vec3 min = crb->CollisionAabb.Position - crb->CollisionAabb.Size / 2.f;
	glm::vec3 max = crb->CollisionAabb.Position + crb->CollisionAabb.Size / 2.f;

	min = roundToGrid(min);
	max = roundToGrid(max);

	if (min == max)
	{
		cw->SpatialHash[min].push_back(crb->Object.TargetId);
		crb->SpatialHashPoints = { min };
		return;
	}

	for (float x = min.x; x <= max.x; x += SPATIAL_HASH_GRID_SIZE)
		for (float y = min.y; y <= max.y; y += SPATIAL_HASH_GRID_SIZE)
			for (float z = min.z; z <= max.z; z += SPATIAL_HASH_GRID_SIZE)
			{
				glm::vec3 point = { x, y, z };
				assert(roundToGrid(point) == point);

				cw->SpatialHash[point].push_back(crb->Object.TargetId);
				crb->SpatialHashPoints.push_back(point);
			}
}

class RigidBodyManager : public ComponentManager<EcRigidBody>
{
public:
	virtual uint32_t CreateComponent(GameObject* Object) override
	{
		uint32_t id = ComponentManager<EcRigidBody>::CreateComponent(Object);
		m_Components[id].Object = Object;
		updateSpatialHash(&m_Components[id]);

		return id;
	}

	virtual const Reflection::StaticPropertyMap& GetProperties() override
	{
		static const Reflection::StaticPropertyMap props = {
			REFLECTION_PROPERTY_SIMPLE(EcRigidBody, PhysicsDynamics, Boolean),
			REFLECTION_PROPERTY_SIMPLE(EcRigidBody, PhysicsRotations, Boolean),

			REFLECTION_PROPERTY(
				"PhysicsCollisions",
				Boolean,
				REFLECTION_PROPERTY_GET_SIMPLE(EcRigidBody, PhysicsCollisions),
				[](void* p, const Reflection::GenericValue& gv)
				{
					EcRigidBody* crb = static_cast<EcRigidBody*>(p);
					crb->PhysicsCollisions = gv.AsBoolean();
					updateSpatialHash(crb);
				}
			),

			REFLECTION_PROPERTY(
				"CollisionFidelity",
				Integer,
				[](void* p)
				-> Reflection::GenericValue
				{
					return static_cast<uint32_t>(static_cast<EcRigidBody*>(p)->CollisionMode);
				},
				[](void* p, const Reflection::GenericValue& gv)
				{
					static_cast<EcRigidBody*>(p)->CollisionMode = static_cast<EnCollisionMode>(gv.AsInteger());
				}
			),

			REFLECTION_PROPERTY_SIMPLE(EcRigidBody, LinearVelocity, Vector3),
			REFLECTION_PROPERTY_SIMPLE(EcRigidBody, AngularVelocity, Vector3),
			REFLECTION_PROPERTY_SIMPLE(EcRigidBody, Friction, Double),
			REFLECTION_PROPERTY_SIMPLE(EcRigidBody, Restitution, Double),
			REFLECTION_PROPERTY_SIMPLE(EcRigidBody, Elasticity, Double),
			REFLECTION_PROPERTY_SIMPLE(EcRigidBody, GravityFactor, Double),

			REFLECTION_PROPERTY(
				"Density",
				Double,
				REFLECTION_PROPERTY_GET_SIMPLE(EcRigidBody, Density),
				[](void* p, const Reflection::GenericValue& gv)
				{
					EcRigidBody* crb = static_cast<EcRigidBody*>(p);
					float dens = (float)gv.AsDouble();

					if (dens < 0.001f)
						RAISE_RT("Minimum density is 0.001");

					crb->Density = dens;
					crb->Mass = dens * crb->CollisionAabb.Size.x * crb->CollisionAabb.Size.y * crb->CollisionAabb.Size.z;
				}
			),
		};

		return props;
	}
};

static RigidBodyManager Instance;

void EcRigidBody::RecomputeAabb()
{
	ZoneScoped;

	EcTransform* ct = this->Object->FindComponent<EcTransform>();
	const glm::mat4& transform = ct->Transform;
	const glm::vec3& glmsize = ct->Size;

	std::array<glm::vec3, 8> verts;

	int i = 0;
	for (int x : {-1, 1})
		for (int y : {-1, 1})
			for (int z : {-1, 1})
				verts[i++] = transform * glm::vec4(glmsize * glm::vec3(x, y, z), 1.f);

	glm::vec3 max = glm::vec3(-FLT_MAX);
	glm::vec3 min = glm::vec3( FLT_MAX);

	for (const glm::vec3& v : verts)
	{
		max = glm::max(max, v);
		min = glm::min(min, v);
	}

	this->CollisionAabb.Position = (min + max) / 2.f;
	this->CollisionAabb.Size = (max - min) / 2.f;

	if (CollisionAabb.Size.x > 10000.f || CollisionAabb.Size.y > 10000.f || CollisionAabb.Size.z > 10000.f)
	{
		if (PhysicsCollisions)
		{
			Log::WarningF("Object '{}' had a Collision Size in one or more dimensions greater than 10,000. Collisions will be disabled", Object->GetFullName());
			PhysicsCollisions = false;
		}
	}

	updateSpatialHash(this);

	this->Mass = Density * CollisionAabb.Size.x * CollisionAabb.Size.y * CollisionAabb.Size.z;
}
