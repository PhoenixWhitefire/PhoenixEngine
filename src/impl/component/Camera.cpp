#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <tracy/Tracy.hpp> 

#include "component/Camera.hpp"
#include "UserInput.hpp"

glm::mat4 EcCamera::GetMatrixForAspectRatio(float AspectRatio) const
{
	glm::vec3 position = glm::vec3(this->Transform[3]);
	glm::vec3 forwardVec = glm::vec3(this->Transform[2]);

	glm::mat4 projectionMatrix = glm::perspective(
		glm::radians(this->FieldOfView),
		AspectRatio,
		this->NearPlane,
		this->FarPlane
	);
	glm::mat4 viewMatrix = glm::lookAt(
		position,
		position + forwardVec,
		glm::vec3(this->Transform[1])
	);

	return projectionMatrix * viewMatrix;
}

class CameraManager : public BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject*) override
    {
        m_Components.emplace_back();
        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    virtual std::vector<void*> GetComponents() override
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (EcCamera& t : m_Components)
            v.push_back((void*)&t);
        
        return v;
    }

	virtual void* GetComponent(uint32_t Id) override
	{
		return &m_Components[Id];
	}

    virtual void DeleteComponent(uint32_t) override
    {
        // TODO id reuse with handles that have a counter per re-use to reduce memory growth
    }

    virtual const Reflection::PropertyMap& GetProperties() override
    {
        static const Reflection::PropertyMap props = 
        {
			EC_PROP_SIMPLE(EcCamera, UseSimpleController, Boolean),
			EC_PROP_SIMPLE(EcCamera, FieldOfView, Double),
			EC_PROP_SIMPLE(EcCamera, Transform, Matrix)
        };

        return props;
    }

    virtual const Reflection::MethodMap& GetMethods() override
    {
        static const Reflection::MethodMap funcs = {};
        return funcs;
    }

    CameraManager()
    {
        GameObject::s_ComponentManagers[(size_t)EntityComponent::Camera] = this;
    }

private:
    std::vector<EcCamera> m_Components;
};

static inline CameraManager Instance{};
