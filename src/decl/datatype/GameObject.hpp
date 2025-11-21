#pragma once

#include <stdint.h>

#define PHX_GAMEOBJECT_NULL_ID UINT32_MAX

#define EC_GET_SIMPLE(c, n) [](void* p)->Reflection::GenericValue { return static_cast<c*>(p)->n; }
#define EC_GET_SIMPLE_TGN(c, n) [](void* p)->Reflection::GenericValue { return static_cast<c*>(p)->n.ToGenericValue(); }
#define EC_SET_SIMPLE(c, n, t) [](void* p, const Reflection::GenericValue& gv)->void { static_cast<c*>(p)->n = gv.As##t(); }
#define EC_SET_SIMPLE_CTOR(c, n, t) [](void* p, const Reflection::GenericValue& gv)->void { static_cast<c*>(p)->n = t(gv); }

#define EC_PROP(strn, t, g, s) { strn, { Reflection::ValueType::t, (Reflection::PropertyGetter)g, (Reflection::PropertySetter)s } }

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
	None,

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
	Bone,
	Example,
	TreeLink,
	
	__count
};

// component type and ID
// if type is "None", the ID is a Game Object ID
struct ReflectorRef
{
	void* Referred() const;

	uint32_t Id = UINT32_MAX;
	EntityComponent Type = EntityComponent::None;

	bool operator == (const ReflectorRef& Other) const
	{
		return Type == Other.Type && Id == Other.Id;
	}

	bool operator != (const ReflectorRef& Other) const
	{
		return !(*this == Other);
	}
};

static inline const std::string_view s_EntityComponentNames[] = 
{
	"<NONE>",
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
	"Model",
	"Bone",
	"Example",
	"TreeLink"
};

static inline const std::unordered_map<std::string_view, EntityComponent> s_ComponentNameToType = 
{
	{ "<NONE>", EntityComponent::None },
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
	{ "Model", EntityComponent::Model },
	{ "Bone", EntityComponent::Bone },
	{ "Example", EntityComponent::Example },
	{ "TreeLink", EntityComponent::TreeLink }
};

static_assert(std::size(s_EntityComponentNames) == (size_t)EntityComponent::__count);

class GameObject;

// Check `ComponentManager` at the bottom of this file
class IComponentManager
{
public:
	virtual uint32_t CreateComponent(GameObject* /* Object */) = 0;
	virtual void UpdateComponent(uint32_t, double) {};
	virtual std::vector<void*> GetComponents() = 0;
	virtual void ForEachComponent(const std::function<bool(void*)>) = 0;
	virtual void* GetComponent(uint32_t) = 0;
	virtual void DeleteComponent(uint32_t /* ComponentId */) = 0;
	virtual void Shutdown() {};

	virtual const Reflection::StaticPropertyMap& GetProperties() = 0;
	virtual const Reflection::StaticMethodMap& GetMethods() = 0;
	virtual const Reflection::StaticEventMap& GetEvents() { static Reflection::StaticEventMap e{}; return e; };
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
	static inline Memory::vector<GameObject, MEMCAT(GameObject)> s_WorldArray{};
	static inline std::array<IComponentManager*, (size_t)EntityComponent::__count> s_ComponentManagers{};

	template <class T>
	T* FindComponent()
	{
		EntityComponent type = T::Type;
		for (const ReflectorRef& ref : Components)
			if (ref.Type == type)
				return static_cast<T*>(GameObject::s_ComponentManagers[(size_t)type]->GetComponent(ref.Id));

		return nullptr;
	}
	uint32_t AddComponent(EntityComponent Type);
	void RemoveComponent(EntityComponent Type);
	void* FindComponentByType(EntityComponent);

	const Reflection::PropertyDescriptor* FindProperty(const std::string_view&, ReflectorRef* Reflector = nullptr);
	const Reflection::MethodDescriptor* FindMethod(const std::string_view&, ReflectorRef* Reflector = nullptr);
	const Reflection::EventDescriptor* FindEvent(const std::string_view&, ReflectorRef* Reflector = nullptr);

	Reflection::GenericValue GetPropertyValue(const std::string_view&);
	void SetPropertyValue(const std::string_view&, const Reflection::GenericValue&);

	std::vector<Reflection::GenericValue> CallMethod(const std::string_view&, const std::vector<Reflection::GenericValue>&);

	Reflection::PropertyMap GetProperties() const;
	Reflection::MethodMap GetMethods() const;
	Reflection::EventMap GetEvents() const;

	// the engine will NEED some objects to continue existing without being
	// de-alloc'd, if only for a loop (`updateScripts`)
	void IncrementHardRefs();
	void DecrementHardRefs(); // de-alloc here when refcount drops to 0

	// won't necessarily de-alloc the object if the engine has any
	// hard refs to it 24/12/2024
	void Destroy();

	// preferable to use this instead of `::GetChildren`/`::GetDescendants`
	// because it does not require any memory allocations
	void ForEachChild(const std::function<bool(GameObject*)>&);
	std::vector<GameObject*> GetChildren();
	std::vector<GameObject*> GetDescendants();

	GameObject* GetParent() const;
	GameObject* FindChild(const std::string_view&);
	GameObject* FindChildWithComponent(EntityComponent);
	
	bool IsDescendantOf(GameObject*);

	std::string GetFullName() const;

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
	bool InDataModel = false;
	bool InWorkspace = false;

	Memory::vector<uint32_t, MEMCAT(GameObject)> Children;
	Memory::vector<ReflectorRef, MEMCAT(GameObject)> Components;
	Reflection::Api ComponentApis{};
	Memory::unordered_map<std::string_view, ReflectorRef, MEMCAT(GameObject)> MemberToComponentMap;

	static nlohmann::json DumpApiToJson();

private:
	static void s_AddObjectApi();

	static inline Reflection::StaticApi s_Api{};

	uint16_t m_HardRefCount = 0;
};

// `GameObject*` directly has the least overhead. `ObjectRef`, a little more,
// and `ObjectHandle`, more than that.
// Use `GameObject*` when you know the World Array won't get re-allocated
// for the scope of your variable.
// Use `ObjectRef` when you know nothing will try to delete your Object.
// Use `ObjectHandle` when you can't trust anybody.
// 
// 20/09/2025

// `ObjectRef`: a weak reference to a GameObject
struct ObjectRef
{
	ObjectRef() = default;

	ObjectRef(GameObject* Object)
	{
		if (Object)
		{
			assert(Object->Valid);
			TargetId = Object->ObjectId;
		}
		else
			TargetId = PHX_GAMEOBJECT_NULL_ID;
	}

	GameObject* Referred() const
	{
		return GameObject::GetObjectById(TargetId);
	}

	bool IsValid() const
	{
		return GameObject::GetObjectById(TargetId) != nullptr;
	}

	bool operator == (const ObjectRef& them) const
	{
		return TargetId == them.TargetId;
	}

	GameObject* operator -> () const
	{
		GameObject* object = Referred();
		assert(object);

		return object;
	}

	operator GameObject* () const
	{
		return Referred();
	}

	uint32_t TargetId = PHX_GAMEOBJECT_NULL_ID;
};

// `ObjectHandle`: a strong reference to a GameObject, prevents it from being de-alloc'd
struct ObjectHandle
{
	ObjectRef Reference;

	ObjectHandle() = default;

	ObjectHandle(GameObject* Object)
	{
		Reference = Object;
		
		if (Object)
			Object->IncrementHardRefs();
	}

	ObjectHandle(const ObjectRef& Other)
	{
		if (GameObject* prevObj = Reference.Referred())
			prevObj->DecrementHardRefs();

		Reference = Other;

		if (GameObject* newObject = Reference.Referred())
			newObject->IncrementHardRefs();
	}

	ObjectHandle(const ObjectHandle& Other)
	{
		if (GameObject* prevObj = Reference.Referred())
			prevObj->DecrementHardRefs();

		Reference = Other.Reference;

		if (GameObject* newObj = Reference.Referred())
			newObj->IncrementHardRefs();
	}
	ObjectHandle(const ObjectHandle&& Other)
	{
		if (GameObject* prevObj = Reference.Referred())
			prevObj->DecrementHardRefs();

		Reference = Other.Reference;

		if (GameObject* newObj = Reference.Referred())
			newObj->IncrementHardRefs();
	}

	~ObjectHandle()
	{
		if (GameObject* obj = Reference.Referred())
		{
			obj->DecrementHardRefs();
			Reference = nullptr;
		}
	}

	bool HasValue() const
	{
		return Reference.TargetId != PHX_GAMEOBJECT_NULL_ID;
	}

	GameObject* Dereference() const
	{
		// Double-free'd or `::Dereference` called on a default-constructed Ref
		assert(HasValue());

		GameObject* object = Reference.Referred();
		assert(object);

		return object;
	}

	void Clear()
	{
		if (HasValue())
			Dereference()->DecrementHardRefs();

		Reference = nullptr;
	}

	bool operator == (const ObjectHandle& them) const
	{
		return Reference == them.Reference;
	}

	GameObject* operator -> () const
	{
		return Dereference();
	}

	ObjectHandle& operator = (const ObjectHandle&& ref)
	{
		if (HasValue())
			Dereference()->DecrementHardRefs();

		Reference = ref.Reference;

		if (HasValue())
			Dereference()->IncrementHardRefs();

		return *this;
	}
	ObjectHandle& operator = (const ObjectHandle& ref)
	{
		if (HasValue())
			Dereference()->DecrementHardRefs();

		Reference = ref.Reference;

		if (HasValue())
			Dereference()->IncrementHardRefs();

		return *this;
	}

	operator GameObject* () const
	{
		return Dereference();
	}
};

template <class T>
class ComponentManager : public IComponentManager
{
public:
	virtual uint32_t CreateComponent(GameObject*) override
	{
		m_Components.emplace_back();
		assert(m_Components.back().Valid);
		return static_cast<uint32_t>(m_Components.size() - 1);
	}

	virtual void* GetComponent(uint32_t Id) override
	{
		T& component = m_Components.at(Id);
		return component.Valid ? (void*)&component : nullptr;;
	}

	virtual std::vector<void*> GetComponents() override
	{
		std::vector<void*> v;
        v.reserve(m_Components.size());

        for (T& component : m_Components)
			if (component.Valid)
            	v.push_back((void*)&component);
        
        return v;
	}

	virtual void ForEachComponent(const std::function<bool(void*)> Continue) override
	{
		for (T& component : m_Components)
			if (component.Valid && !Continue((void*)&component))
				break;
	}

	virtual void UpdateComponent(uint32_t, double) override {};

	virtual void DeleteComponent(uint32_t Id) override
	{
		T& component = m_Components.at(Id);
		component.Valid = false;
	}

	virtual void Shutdown() override
	{
		m_Components.clear();
	}

	virtual const Reflection::StaticPropertyMap& GetProperties() override
	{
		static const Reflection::StaticPropertyMap properties;
		return properties;
	}

	virtual const Reflection::StaticMethodMap& GetMethods() override
	{
		static const Reflection::StaticMethodMap methods;
		return methods;
	}

	virtual const Reflection::StaticEventMap& GetEvents() override
	{
		static const Reflection::StaticEventMap events;
		return events;
	}

	ComponentManager()
	{
		GameObject::s_ComponentManagers[(size_t)T::Type] = this;
	}

	std::vector<T> m_Components;
};
