// 15/09/2024
// A template/example GameObject

#include <glm/vec3.hpp>
#include <string>

#include "datatype/GameObject.hpp"

struct EcExample : public Component<EntityComponent::Example>
{
	bool SuperCoolBool = true;
	int64_t SomeInteger = 64;
	float MaybeDeliciousPi = 3.141592f;
	std::string SecretMessage = "Love is Love!";
	std::string EvenMoreSecretMessage = "57";
	glm::vec3 WhereIAm{};

	std::vector<Reflection::EventCallback> OnGreetedCallbacks;

	ObjectRef Object;
	bool Valid = true;
};
