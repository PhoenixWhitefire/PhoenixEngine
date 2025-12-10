#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>

#include "component/Camera.hpp"
#include "component/Transform.hpp"
#include "UserInput.hpp"

class CameraManager : public ComponentManager<EcCamera>
{
public:
	uint32_t CreateComponent(GameObject* Object) override
	{
		if (!Object->FindComponent<EcTransform>())
			Object->AddComponent(EntityComponent::Transform);

		m_Components.emplace_back();
		m_Components.back().Object = Object;

		return static_cast<uint32_t>(m_Components.size() - 1);
	}

    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = {
			REFLECTION_PROPERTY_SIMPLE(EcCamera, UseSimpleController, Boolean),
			REFLECTION_PROPERTY_SIMPLE(EcCamera, FieldOfView, Double)
        };

        return props;
    }
};

static inline CameraManager Instance;

glm::mat4 EcCamera::GetRenderMatrix(float AspectRatio) const
{
	glm::mat4 trans = GetWorldTransform();

	glm::mat4 projectionMatrix = glm::perspective(
		glm::radians(this->FieldOfView),
		AspectRatio,
		this->NearPlane,
		this->FarPlane
	);
	glm::mat4 viewMatrix = glm::lookAt(
		glm::vec3(trans[3]),
		glm::vec3(trans[3]) + glm::vec3(trans[2]),
		glm::vec3(trans[1])
	);

	return projectionMatrix * viewMatrix; /* glm::inverse(trans); */
}

glm::mat4 EcCamera::GetWorldTransform() const
{
	if (EcTransform* et = (Object.Referred() ? Object->FindComponent<EcTransform>() : nullptr))
		return et->Transform;
	else
		return glm::mat4(1.f);
}

void EcCamera::SetWorldTransform(const glm::mat4& NewTrans)
{
	if (EcTransform* et = (Object.Referred() ? Object->FindComponent<EcTransform>() : nullptr))
		et->SetWorldTransform(NewTrans);
}
