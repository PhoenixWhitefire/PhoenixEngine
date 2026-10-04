// LuauData.cpp, 04/10/2026
#include <lua.h>

#include "component/LuauData.hpp"

void LuauDataComponentManager::DeleteComponent(uint32_t Id)
{
    EcLuauData& el = Components[Id];

    for (const EcLuauData::LuauComponent& component : el.ComponentData)
        lua_unref(component.VM, component.Id);

    el.ComponentData.clear();
    ComponentManager<EcLuauData>::DeleteComponent(Id);
}
