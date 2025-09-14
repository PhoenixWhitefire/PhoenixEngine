#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>

#include "component/Camera.hpp"
#include "UserInput.hpp"

glm::mat4 EcCamera::GetMatrixForAspectRatio(float AspectRatio) const
{
	glm::vec3 position = glm::vec3(this->Transform[3]);
	glm::vec3 forwardVec = glm::vec3(this->Transform[2]);
    glm::vec3 upVec = glm::vec3(this->Transform[1]);

	glm::mat4 projectionMatrix = glm::perspective(
		glm::radians(this->FieldOfView),
		AspectRatio,
		this->NearPlane,
		this->FarPlane
	);
	glm::mat4 viewMatrix = glm::lookAt(
		position,
		position + forwardVec,
		upVec
	);

	return projectionMatrix * viewMatrix;
}

class CameraManager : public ComponentManager<EcCamera>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = 
        {
			EC_PROP_SIMPLE(EcCamera, UseSimpleController, Boolean),
			EC_PROP_SIMPLE(EcCamera, FieldOfView, Double),
			EC_PROP_SIMPLE(EcCamera, Transform, Matrix)
        };

        return props;
    }
};

static inline CameraManager Instance{};
