#pragma once

#include <stdint.h>

#define PHX_GAMEOBJECT_NULL_ID UINT32_MAX

#include <unordered_map>
#include <string_view>
#include <functional>
#include <vector>
#include <nljson.hpp>

#include "Reflection.hpp"
#include "Utilities.hpp"
#include "Stl.hpp"

#include "datatype/EntityComponent.hpp"
#include "datatype/ComponentBase.hpp"

class GameObject
{
public:
	static GameObject* FromGenericValue(const Reflection::GenericValue&);

	// create with a component
	static GameObject* Create(EntityComponent);
	static GameObject* Create(const std::string_view&);
	// create empty object
	static GameObject* Create();

	static GameObject* GetObjectById(uint32_t);

	static inline uint32_t s_DataModel = PHX_GAMEOBJECT_NULL_ID;
	static inline hx::vector<GameObject, MEMCAT(GameObject)> s_WorldArray;
	static inline std::array<IComponentManager*, (size_t)EntityComponent::__count> s_ComponentManagers;

	struct Collection
	{
		std::string Name;
		std::vector<uint32_t> Items;

		struct Event
		{
			// We want these to exist in the same memory location for the entire lifetime of the Engine
			Reflection::EventDescriptor* Descriptor = nullptr;
			std::vector<Reflection::EventCallback> Callbacks;
		};

		Event AddedEvent;
		Event RemovedEvent;

		uint16_t Id = UINT16_MAX;
	};

	static inline std::vector<Collection> s_Collections;
	static inline std::unordered_map<std::string, uint16_t> s_CollectionNameToId;

	// Returns a reference to a Collection, creating one if it does not exist
	static Collection& GetCollection(const std::string&);

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

	void AddTag(const std::string&);
	void RemoveTag(const std::string&);
	bool HasTag(const std::string&);

	const Reflection::PropertyDescriptor* FindProperty(const std::string_view&, ReflectorRef* Reflector = nullptr);
	const Reflection::MethodDescriptor* FindMethod(const std::string_view&, ReflectorRef* Reflector = nullptr);
	const Reflection::EventDescriptor* FindEvent(const std::string_view&, ReflectorRef* Reflector = nullptr);

	Reflection::GenericValue GetPropertyValue(const std::string_view&);
	void SetPropertyValue(const std::string_view&, const Reflection::GenericValue&);
	Reflection::GenericValue GetDefaultPropertyValue(const std::string_view&);

	std::vector<Reflection::GenericValue> CallMethod(const std::string_view&, const std::vector<Reflection::GenericValue>&, const Logging::Context&);

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
	bool ForEachDescendant(const std::function<bool(GameObject*)>&);
	std::vector<GameObject*> GetChildren();
	std::vector<GameObject*> GetDescendants();

	GameObject* GetParent() const;
	GameObject* FindChild(const std::string_view&);
	GameObject* FindChildWithComponent(EntityComponent);
	
	bool IsDescendantOf(const GameObject*) const;

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

	std::string Name = "GameObject";

	uint32_t ObjectId = PHX_GAMEOBJECT_NULL_ID;
	uint32_t Parent = PHX_GAMEOBJECT_NULL_ID;
	uint32_t OwningDataModel = PHX_GAMEOBJECT_NULL_ID;
	uint32_t OwningWorkspace = PHX_GAMEOBJECT_NULL_ID;

	bool Enabled = true;
	bool Serializes = true;
	bool IsDestructionPending = false;
	bool Valid = true;

	std::vector<uint32_t> Children;
	std::vector<ReflectorRef> Components;
	Reflection::Api ComponentApis;
	std::unordered_map<std::string_view, ReflectorRef> MemberToComponentMap;
	std::vector<uint16_t> Tags;
	std::vector<Reflection::EventCallback> OnTagAddedCallbacks;
	std::vector<Reflection::EventCallback> OnTagRemovedCallbacks;

	static nlohmann::json DumpApiToJson();
	static const Reflection::StaticApi s_Api;

private:
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
