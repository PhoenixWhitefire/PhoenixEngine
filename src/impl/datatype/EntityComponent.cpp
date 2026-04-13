#include "datatype/EntityComponent.hpp"
#include "datatype/ComponentBase.hpp"
#include "datatype/GameObject.hpp"
#include "datatype/ComponentDependencies.hpp"

const std::unordered_map<std::string_view, EntityComponent> s_ComponentNameToType = []()
{
	std::unordered_map<std::string_view, EntityComponent> map;
	map.reserve((size_t)EntityComponent::__count);

	for (size_t i = 0; i < (size_t)EntityComponent::__count; i++)
		map[s_EntityComponentNames[i]] = (EntityComponent)i;

	return map;
}();

EntityComponent FindComponentTypeByName(const std::string_view& Name)
{
	const auto& it = s_ComponentNameToType.find(Name);

	if (it == s_ComponentNameToType.end())
		return EntityComponent::None;
	else
		return it->second;
}

void RegisterComponentManager(EntityComponent Type, IComponentManager* Manager)
{
	assert(GameObjectManager::Get()->ComponentManagers[(size_t)Type] == nullptr);
	GameObjectManager::Get()->ComponentManagers[(size_t)Type] = Manager;
}

IComponentManager* GetComponentManagerByComponentType(EntityComponent Type)
{
	assert(GameObjectManager::Get()->ComponentManagers[(size_t)Type] != nullptr);
	return GameObjectManager::Get()->ComponentManagers[(size_t)Type];
}

const std::span<const EntityComponent> GetCommonDependenciesForComponent(EntityComponent ec)
{
	const auto& it = s_ComponentCommonDependencies.find(ec);
	if (it == s_ComponentCommonDependencies.end())
		return {};

	return it->second;
}
