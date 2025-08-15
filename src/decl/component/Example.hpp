// 15/09/2024
// A template/example GameObject

#include <glm/vec3.hpp>
#include <string>

#include "datatype/GameObject.hpp"

struct EcExample
{
	bool Value1 = true;
	int64_t Value2 = 64;
	float Value3 = 3.141592f;
	std::string Value4 = "Love is Love!";
	std::string Value5 = "I am NOT homosexual";
	glm::vec3 Value6{};

	std::vector<Reflection::EventCallback> OnGreetedCallbacks;

	GameObjectRef Object;
};
