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
	template <class T>
	T* FindComponent()
	{
		EntityComponent type = T::Type;
		for (const ReflectorRef& ref : Components)
			if (ref.Type == type)
				return static_cast<T*>(GetComponentManagerByComponentType(type)->GetComponent(ref.Id));

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
	void ForEachChild(const std::function<bool(const ObjectHandle&)>&);
	bool ForEachDescendant(const std::function<bool(const ObjectHandle&)>&);
	std::vector<ObjectHandle> GetChildren();
	std::vector<ObjectHandle> GetDescendants();

	GameObject* GetParent() const;
	GameObject* FindChild(const std::string_view&);
	GameObject* FindChildWithComponent(EntityComponent);

	bool IsDescendantOf(const GameObject*) const;

	std::string GetFullName() const;

	void SetParent(const ObjectHandle&);
	void AddChild(const ObjectHandle&);
	void RemoveChild(uint32_t);

	// performs a 1-1 copy, including copying the `Parent` property
	ObjectHandle Duplicate();

	static Reflection::GenericValue s_ToGenericValue(GameObject*);
	Reflection::GenericValue ToGenericValue();

	std::string Name = "GameObject";

	uint32_t ObjectId = PHX_GAMEOBJECT_NULL_ID;
	uint32_t Parent = PHX_GAMEOBJECT_NULL_ID;
	uint32_t OwningDataModel = PHX_GAMEOBJECT_NULL_ID;
	uint32_t OwningWorkspace = PHX_GAMEOBJECT_NULL_ID;

	std::vector<uint32_t> Children;
	std::vector<ReflectorRef> Components;
	Reflection::Api ComponentApis;
	std::unordered_map<std::string_view, ReflectorRef> MemberToComponentMap;
	std::vector<uint16_t> Tags;
	std::vector<Reflection::EventCallback> OnTagAddedCallbacks;
	std::vector<Reflection::EventCallback> OnTagRemovedCallbacks;

	uint16_t HardRefCount = 0;

	bool Enabled = true;
	bool Serializes = true;
	bool IsDestructionPending = false;
	bool Valid = true;

	static nlohmann::json DumpApiToJson();
	static const Reflection::StaticApi s_Api;
};

class GameObjectManager
{
public:
	GameObjectManager();
	~GameObjectManager();

	static GameObjectManager* Get();

	// create with a component
	ObjectHandle Create(EntityComponent);
	ObjectHandle Create(const std::string_view&);
	// create empty object
	ObjectHandle Create();

	template <class T>
	static ObjectHandle s_Create(T A)
	{
		return Get()->Create(A);
	}

	GameObject* FindById(uint32_t);
	GameObject* FromGenericValue(const Reflection::GenericValue&);

	uint32_t DataModel = PHX_GAMEOBJECT_NULL_ID;
	hx::vector<GameObject, MEMCAT(GameObject)> WorldArray;
	std::array<IComponentManager*, (size_t)EntityComponent::__count> ComponentManagers{};

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

	std::vector<Collection> Collections;
	std::unordered_map<std::string, uint16_t> CollectionNameToId;

	// Returns a reference to a Collection, creating one if it does not exist
	Collection& GetCollection(const std::string&);
};
