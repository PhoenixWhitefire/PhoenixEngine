#pragma once

#include <stdint.h>

#define PHX_GAMEOBJECT_NULL_ID UINT32_MAX

#define EC_GET_SIMPLE(c, n) [](void* p)->Reflection::GenericValue { return static_cast<c*>(p)->n; }
#define EC_GET_SIMPLE_TGN(c, n) [](void* p)->Reflection::GenericValue { return static_cast<c*>(p)->n.ToGenericValue(); }
#define EC_SET_SIMPLE(c, n, t) [](void* p, const Reflection::GenericValue& gv)->void { static_cast<c*>(p)->n = gv.As##t(); }
#define EC_SET_SIMPLE_CTOR(c, n, t) [](void* p, const Reflection::GenericValue& gv)->void { static_cast<c*>(p)->n = t(gv); }

#define EC_PROP(strn, t, g, s) { strn, { Reflection::ValueType::t, g, s } }

#define EC_PROP_SIMPLE(c, n, t) EC_PROP(#n, t, EC_GET_SIMPLE(c, n), EC_SET_SIMPLE(c, n, t))
#define EC_PROP_SIMPLE_NGV(c, n, t) EC_PROP(#n, t, EC_GET_SIMPLE_TGN(c, n), EC_SET_SIMPLE_CTOR(c, n, t))

#include <unordered_map>
#include <string_view>
#include <functional>
#include <vector>
#include <nljson.hpp>

#include "Reflection.hpp"
#include "Utilities.hpp"
#include "Memory.hpp"

enum class EntityComponent : uint8_t
{
	Transform,
	Mesh,
	ParticleEmitter,
	Script,
	Sound,
	Workspace,
	DataModel,
	PointLight,
	DirectionalLight,
	SpotLight,
	Camera,
	Animation,
	Model,
	
	__count
};

static inline const std::string_view s_EntityComponentNames[] = 
{
	"Transform",
	"Mesh",
	"ParticleEmitter",
	"Script",
	"Sound",
	"Workspace",
	"DataModel",
	"PointLight",
	"DirectionalLight",
	"SpotLight",
	"Camera",
	"Animation",
	"Model"
};

static inline const std::unordered_map<std::string_view, EntityComponent> s_ComponentNameToType = 
{
	{ "Transform", EntityComponent::Transform },
	{ "Mesh", EntityComponent::Mesh },
	{ "ParticleEmitter", EntityComponent::ParticleEmitter },
	{ "Script", EntityComponent::Script },
	{ "Sound", EntityComponent::Sound },
	{ "Workspace", EntityComponent::Workspace },
	{ "DataModel", EntityComponent::DataModel },
	{ "PointLight", EntityComponent::PointLight },
	{ "DirectionalLight", EntityComponent::DirectionalLight },
	{ "SpotLight", EntityComponent::SpotLight },
	{ "Camera", EntityComponent::Camera },
	{ "Animation", EntityComponent::Animation },
	{ "Model", EntityComponent::Model }
};

static_assert(std::size(s_EntityComponentNames) == (size_t)EntityComponent::__count);

void* ComponentHandleToPointer(const std::pair<EntityComponent, uint32_t>& Handle);

class GameObject;

class BaseComponentManager
{
public:
	virtual uint32_t CreateComponent(GameObject* /* Object */) { throw("Not implemented"); };
	virtual void UpdateComponent(uint32_t, double) { throw("Not implemented"); };
	virtual std::vector<void*> GetComponents() { throw("Not implemented"); };
	virtual void* GetComponent(uint32_t) { throw("Not implemented"); };
	virtual void DeleteComponent(uint32_t /* ComponentId */) { throw("Not implemented"); };

	virtual const Reflection::PropertyMap& GetProperties() { throw("Not implemented"); };
	virtual const Reflection::FunctionMap& GetFunctions() { throw("Not implemented"); };
};

class GameObject
{
public:
	GameObject();

	static GameObject* FromGenericValue(const Reflection::GenericValue&);

	static bool IsValidClass(const std::string_view&);
	// create with a component
	static GameObject* Create(EntityComponent);
	static GameObject* Create(const std::string_view&);
	// create empty object
	static GameObject* Create();

	static GameObject* GetObjectById(uint32_t);

	static inline uint32_t s_DataModel = PHX_GAMEOBJECT_NULL_ID;
	static inline std::vector<GameObject> s_WorldArray{};
	static inline std::array<BaseComponentManager*, (size_t)EntityComponent::__count> s_ComponentManagers{};

	uint32_t AddComponent(EntityComponent Type);
	void RemoveComponent(EntityComponent Type);
	template <class T> T* GetComponent()
	{
		EntityComponent type = T::Type;
		for (const std::pair<EntityComponent, uint32_t>& pair : m_Components)
			if (pair.first == type)
				return static_cast<T*>(GameObject::s_ComponentManagers[(size_t)type]->GetComponent(pair.second));
		
		return nullptr;
	}
	void* GetComponentByType(EntityComponent);
	std::vector<std::pair<EntityComponent, uint32_t>>& GetComponents();

	Reflection::Property* FindProperty(const std::string_view&, bool* FromObject = nullptr);
	Reflection::Function* FindFunction(const std::string_view&, bool* FromObject = nullptr);
	Reflection::GenericValue GetPropertyValue(const std::string_view&);
	void SetPropertyValue(const std::string_view&, const Reflection::GenericValue&);
	std::vector<Reflection::GenericValue> CallFunction(const std::string_view&, const std::vector<Reflection::GenericValue>&);

	Reflection::PropertyMap GetProperties() const;
	Reflection::FunctionMap GetFunctions() const;

	// the engine will NEED some objects to continue existing without being
	// de-alloc'd, if only for a loop (`updateScripts`)
	void IncrementHardRefs();
	void DecrementHardRefs(); // de-alloc here when refcount drops to 0

	// won't necessarily de-alloc the object if the engine has any
	// hard refs to it 24/12/2024
	void Destroy();

	std::vector<GameObject*> GetChildren();
	std::vector<GameObject*> GetDescendants();

	GameObject* GetParent() const;
	GameObject* FindChild(const std::string_view&);
	// accounts for inheritance
	GameObject* FindChildWhichIsA(const std::string_view&);

	std::string GetFullName() const;
	// whether this object inherits from or is the given class
	bool IsA(const std::string_view&) const;

	void SetParent(GameObject*);
	void AddChild(GameObject*);
	void RemoveChild(uint32_t);

	// performs a 1-1 copy, including copying the `Parent` property
	GameObject* Duplicate();
	// destructively combines the passed object with the current object
	// passed object will be deleted
	void MergeWith(GameObject*);

	static Reflection::GenericValue s_ToGenericValue(const GameObject*);
	Reflection::GenericValue ToGenericValue() const;

	uint32_t ObjectId = PHX_GAMEOBJECT_NULL_ID;
	uint32_t Parent = PHX_GAMEOBJECT_NULL_ID;

	std::string Name = "GameObject";

	bool Enabled = true;
	bool Serializes = true;
	bool IsDestructionPending = false;
	bool Valid = true;

	static nlohmann::json DumpApiToJson();

private:
	static void s_AddObjectApi();

	std::vector<uint32_t> m_Children;
	// component type and ID
	std::vector<std::pair<EntityComponent, uint32_t>> m_Components;
	Reflection::Api m_ComponentApis{};
	std::unordered_map<std::string_view, std::pair<EntityComponent, uint32_t>> m_MemberToComponentMap;

	static inline Reflection::Api s_Api{};

	uint16_t m_HardRefCount = 0;
};

struct GameObjectRef
{
	GameObjectRef() = default;

	GameObjectRef(GameObject* Object)
	: m_TargetId(Object->ObjectId)
	{
		PHX_ENSURE_MSG(m_TargetId != PHX_GAMEOBJECT_NULL_ID, "Tried to create a Hard Reference to an uninitialized/invalid GameObject");

		Object->IncrementHardRefs();
	}

	GameObjectRef(const GameObjectRef& Other)
		: m_TargetId(Other.Contained()->ObjectId)
	{
		Contained()->IncrementHardRefs();
	}
	GameObjectRef(const GameObjectRef&& Other)
		: m_TargetId(Other.Contained()->ObjectId)
	{
		Contained()->IncrementHardRefs();
	}

	~GameObjectRef()
	{
		if (m_TargetId != PHX_GAMEOBJECT_NULL_ID)
			Contained()->DecrementHardRefs();
		m_TargetId = PHX_GAMEOBJECT_NULL_ID;
	}

	GameObject* Contained() const
	{
		// Double-free'd or `::Contained` called on a default-constructed Ref
		assert(m_TargetId != PHX_GAMEOBJECT_NULL_ID);

		GameObject* g = GameObject::GetObjectById(m_TargetId);
		PHX_ENSURE_MSG(g, "Referenced GameObject was de-alloc'd while we wanted to keep it :(");

		return g;
	}

	void Invalidate()
	{
		PHX_ENSURE(m_TargetId != PHX_GAMEOBJECT_NULL_ID);

		m_TargetId = 0;
	}

	uint32_t m_TargetId = PHX_GAMEOBJECT_NULL_ID;

	bool operator == (const GameObjectRef& them) const
	{
		return Contained() == them.Contained();
	}

	GameObject* operator -> () const
	{
		return Contained();
	}

	GameObjectRef& operator = (const GameObjectRef&& ref)
	{
		if (m_TargetId != PHX_GAMEOBJECT_NULL_ID)
			Contained()->DecrementHardRefs();

		m_TargetId = ref->ObjectId;
		Contained()->IncrementHardRefs();

		return *this;
	}
	GameObjectRef& operator = (const GameObjectRef& ref)
	{
		if (m_TargetId != PHX_GAMEOBJECT_NULL_ID)
			Contained()->DecrementHardRefs();
		
		m_TargetId = ref->ObjectId;
		Contained()->IncrementHardRefs();

		return *this;
	}

	GameObjectRef& operator = (GameObject* Object)
	{
		m_TargetId = Object->ObjectId;
		Object->IncrementHardRefs();
		return *this;
	}

	operator GameObject* () const
	{
		return Contained();
	}
};
