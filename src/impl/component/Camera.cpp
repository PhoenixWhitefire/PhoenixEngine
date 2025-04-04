#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <tracy/Tracy.hpp> 

#include "component/Camera.hpp"
#include "datatype/Vector3.hpp"
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

class CameraManager : BaseComponentManager
{
public:
    virtual uint32_t CreateComponent(GameObject* Object) final
    {
        m_Components.emplace_back();
        return static_cast<uint32_t>(m_Components.size() - 1);
    }

    virtual std::vector<void*> GetComponents() final
    {
        std::vector<void*> v;
        v.reserve(m_Components.size());

        for (const EcCamera& t : m_Components)
            v.push_back((void*)&t);
        
        return v;
    }

	virtual void* GetComponent(uint32_t Id) final
	{
		return &m_Components[Id];
	}

    virtual void DeleteComponent(uint32_t Id) final
    {
        // TODO id reuse with handles that have a counter per re-use to reduce memory growth
    }

    virtual const Reflection::PropertyMap& GetProperties() final
    {
        static const Reflection::PropertyMap props = 
        {
			EC_PROP_SIMPLE(EcCamera, UseSimpleController, Boolean),
			EC_PROP_SIMPLE(EcCamera, FieldOfView, Double),
			EC_PROP_SIMPLE(EcCamera, Transform, Matrix)
        };

        return props;
    }

    virtual const Reflection::FunctionMap& GetFunctions() final
    {
        static const Reflection::FunctionMap funcs = {};
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

#if 0
PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(Camera);

static bool s_DidInitReflection = false;

void Object_Camera::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_INHERITAPI(GameObject);

	REFLECTION_DECLAREPROP_SIMPLE(Object_Camera, UseSimpleController, Bool);
	REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(Object_Camera, FieldOfView, Double, float);

	REFLECTION_DECLAREPROP_SIMPLE(Object_Camera, Transform, Matrix);
}

/*
Object_Camera::Object_Camera()
{
	this->Name = "Camera";
	this->ClassName = "Camera";

	this->Transform = glm::lookAt(glm::vec3(), glm::vec3(0.f, 0.f, -1.f), glm::vec3(0.f, 1.f, 0.f));

	s_DeclareReflections();
	ApiPointer = &s_Api;
}

void Object_Camera::Update(double)
{
	if (!this->IsSceneCamera)
		return;

	if (!this->UseSimpleController)
		return;

	// Camera movement code
}

glm::mat4 Object_Camera::GetMatrixForAspectRatio(float AspectRatio) const
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
*/
#endif
