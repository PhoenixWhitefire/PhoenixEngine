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
#include "Memory.hpp"

class GameObject : public Reflection::Reflectable
{
public:
	GameObject();
	virtual ~GameObject() noexcept(false);

	GameObject(GameObject&) = delete;

	static GameObject* FromGenericValue(const Reflection::GenericValue&);

	static bool IsValidObjectClass(const std::string&);
	static GameObject* Create(const std::string&);
	static GameObject* GetObjectById(uint32_t);

	static inline GameObject* s_DataModel{};
	static inline std::vector<GameObject*> s_WorldArray{};

	virtual void Initialize();
	virtual void Update(double DeltaTime);

	// the engine will NEED some objects to continue existing without being
	// de-alloc'd, if only for a loop (`updateScripts`)
	void IncrementHardRefs();
	void DecrementHardRefs(); // de-alloc here when refcount drops to 0

	// won't necessarily de-alloc the object if the engine has any
	// refs to it 24/12/2024
	void Destroy();

	std::vector<GameObject*> GetChildren();
	std::vector<GameObject*> GetDescendants();

	GameObject* GetParent() const;
	GameObject* FindChild(const std::string&);
	// accounts for inheritance
	GameObject* FindChildWhichIsA(const std::string&);

	std::string GetFullName() const;
	// whether this object inherits from or is the given class
	bool IsA(const std::string&) const;

	void SetParent(GameObject*);
	void AddChild(GameObject*);
	void RemoveChild(uint32_t);

	// performs a 1-1 copy, including copying the `Parent` property
	GameObject* Duplicate();
	// destructively combines the passed object with the current object
	// passed object will be deleted
	void MergeWith(GameObject*);

	Reflection::GenericValue ToGenericValue() const;

	uint32_t ObjectId = PHX_GAMEOBJECT_NULL_ID;
	uint32_t Parent = PHX_GAMEOBJECT_NULL_ID;

	std::string Name = "GameObject";
	std::string ClassName = "GameObject";

	bool Enabled = true;
	bool Serializes = true;
	bool IsDestructionPending = false;

	static nlohmann::json DumpApiToJson();

	REFLECTION_DECLAREAPI;
	MEM_ALLOC_OPERATORS(GameObject, GameObject);

	// I followed this StackOverflow post:
	// https://stackoverflow.com/a/582456/16875161

	// Needs to be in at-most `protected` because `RegisterDerivedObject`
	// So sub-classes can access it and register themselves
	typedef std::unordered_map<std::string, GameObject* (*)()> GameObjectMapType;
	static inline GameObjectMapType s_GameObjectMap{};

protected:
	std::vector<uint32_t> m_Children;

private:
	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};

	// i think something fishy would have to be going on
	// for the refcount to be >255
	// 24/12/2024
	uint8_t m_HardRefCount = 0;
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

template <class T> class GameObjectRef
{
public:
	GameObjectRef(T* Object)
	: m_TargetId(Object->ObjectId)
	{
		if (m_TargetId == PHX_GAMEOBJECT_NULL_ID)
			throw("Tried to create a Hard Reference to a GameObject prior to it being `::Initialize`'d");
		else
			Object->IncrementHardRefs();
	}

	GameObjectRef(GameObjectRef& Other)
		: m_TargetId(Other.Contained()->ObjectId)
	{
		this->Contained()->IncrementHardRefs();
	}
	GameObjectRef(GameObjectRef&& Other)
		: m_TargetId(Other.Contained()->ObjectId)
	{
		this->Contained()->IncrementHardRefs();
	}

	~GameObjectRef()
	{
		this->Contained()->DecrementHardRefs();
		m_TargetId = PHX_GAMEOBJECT_NULL_ID;
	}

	T* Contained()
	{
		if (m_TargetId == PHX_GAMEOBJECT_NULL_ID)
			throw("Double-free'd or `::Contained` called on default-constructed GameObjectRef");

		GameObject* g = GameObject::GetObjectById(m_TargetId);

		if (!g)
			throw("Referenced GameObject was de-alloc'd while we wanted to keep it :(");
		else
			return static_cast<T*>(g);
	}

	bool operator == (GameObject* them)
	{
		return static_cast<GameObject*>(Contained()) == them;
	}

	GameObjectRef& operator = (GameObjectRef&&) = delete;
	GameObjectRef& operator = (GameObjectRef&) = delete;

private:
	uint32_t m_TargetId = PHX_GAMEOBJECT_NULL_ID;
};
