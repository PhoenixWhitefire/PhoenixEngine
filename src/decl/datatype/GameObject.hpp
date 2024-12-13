#pragma once

#include <stdint.h>


#define PHX_GAMEOBJECT_NULL_ID UINT32_MAX

// link a class name to a C++ GameObject-deriving class
#define PHX_GAMEOBJECT_LINKTOCLASS(strclassname, cclass) static RegisterDerivedObject<cclass> Register##cclass(strclassname);
// shorthand for `PHX_GAMEOBJECT_LINKTOCLASS`. the resulting class name is the same as passed in,
// and the C++ class is `Object_<Class>`
#define PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(classname) PHX_GAMEOBJECT_LINKTOCLASS(#classname, Object_##classname)

#include <unordered_map>
#include <functional>
#include <vector>
#include <string>
#include <nljson.hpp>

#include "Reflection.hpp"

class GameObject : public Reflection::Reflectable
{
public:
	GameObject();
	virtual ~GameObject();

	GameObject(GameObject&) = delete;

	static GameObject* FromGenericValue(const Reflection::GenericValue&);

	static bool IsValidObjectClass(const std::string&);
	static GameObject* Create(const std::string&);
	static GameObject* GetObjectById(uint32_t);

	static inline GameObject* s_DataModel{};
	static inline std::vector<GameObject*> s_WorldArray{};

	virtual void Initialize();
	virtual void Update(double DeltaTime);

	std::vector<GameObject*> GetChildren();
	std::vector<GameObject*> GetDescendants();

	GameObject* GetParent() const;
	GameObject* GetChild(const std::string&);
	// accounts for inheritance
	GameObject* GetChildOfClass(const std::string&);
	GameObject* GetChildById(uint32_t);

	std::string GetFullName();
	// whether this object inherits from or is the given class
	bool IsA(const std::string&);

	void Destroy();

	void SetParent(GameObject*);
	void AddChild(GameObject*);
	void RemoveChild(uint32_t);

	Reflection::GenericValue ToGenericValue();

	uint32_t ObjectId = 0;
	uint32_t Parent = PHX_GAMEOBJECT_NULL_ID;

	std::string Name = "GameObject";
	std::string ClassName = "GameObject";

	bool Enabled = true;
	bool ParentLocked = false;

	static nlohmann::json DumpApiToJson();

	REFLECTION_DECLAREAPI;

protected:
	std::vector<uint32_t> m_Children;

	// I followed this StackOverflow post:
	// https://stackoverflow.com/a/582456/16875161

	// Needs to be in `protected` because `RegisterDerivedObject`
	// So sub-classes can access it and register themselves
	typedef std::unordered_map<std::string, GameObject* (*)()> GameObjectMapType;
	static inline GameObjectMapType s_GameObjectMap{};

private:
	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};
};

template<typename T> GameObject* constructGameObjectHeir()
{
	return new T;
}

template <typename T>
struct RegisterDerivedObject : GameObject
{
	RegisterDerivedObject(const std::string& ObjectClass)
	{
		s_GameObjectMap.insert(std::make_pair(ObjectClass, &constructGameObjectHeir<T>));
	}
};

typedef GameObject Object_GameObject;
