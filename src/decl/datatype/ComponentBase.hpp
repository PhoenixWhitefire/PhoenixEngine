// ComponentBase.hpp - templates and Component Manager virtual class, 31/01/2026
#pragma once

#include "Reflection.hpp"

class GameObject;

class IComponentManager
{
public:
	virtual uint32_t CreateComponent(GameObject* /* Object */) = 0;
	virtual std::vector<void*> GetComponents() = 0;
	virtual void ForEachComponent(const std::function<bool(void*)>) = 0;
	virtual void* GetComponent(uint32_t) = 0;
	virtual void DeleteComponent(uint32_t /* ComponentId */) = 0;
	virtual void Shutdown() = 0;

	virtual const Reflection::StaticPropertyMap& GetProperties() = 0;
	virtual const Reflection::StaticMethodMap& GetMethods() = 0;
	virtual const Reflection::StaticEventMap& GetEvents() = 0;
	virtual Reflection::GenericValue GetDefaultPropertyValue(const std::string_view&) = 0;
	virtual Reflection::GenericValue GetDefaultPropertyValue(const Reflection::PropertyDescriptor*) = 0;
};

void RegisterComponentManager(EntityComponent Type, IComponentManager*);

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
		return component.Valid ? (void*)&component : nullptr;
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

	virtual Reflection::GenericValue GetDefaultPropertyValue(const std::string_view& Property) override
	{
		return GetDefaultPropertyValue(&GetProperties().at(Property));
	}

	virtual Reflection::GenericValue GetDefaultPropertyValue(const Reflection::PropertyDescriptor* Property) override
	{
		static T Defaults;
		return Property->Get((void*)&Defaults);
	}

	ComponentManager()
	{
		RegisterComponentManager(T::Type, this);
	}

	std::vector<T> m_Components;
};

template <EntityComponent T>
struct Component
{
	static inline EntityComponent Type = T;
};
