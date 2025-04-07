// 15/09/2024
// A template/example GameObject

#if 0

#pragma once

#include "datatype/GameObject.hpp"
#include "datatype/Vector3.hpp"
#include "gameobject/Script.hpp"

class Object_Example : public Object_Script
{
public:
	Object_Example();

	// `Initialize` is called *after* the `ObjectId` has been set,
	// and after the Object has been added to the World Array
	// 10/01/2025
	void Initialize() override;
	//void Update(double) override;

	bool Value1 = true;
	int64_t Value2 = 64;
	float Value3 = 3.141592f;
	std::string Value4 = "The Sun, the Moon and the Stars";
	Vector3 Value5{};

	// Redirects API getters to point to the `s_Api` defined in *this* class
	REFLECTION_DECLAREAPI;
	
private: // Not shared between inheritors
	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};;
};
#endif
