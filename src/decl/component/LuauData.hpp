// LuauData.hpp, 04/10/2026
#pragma once

#include <set>

#include "datatype/ComponentBase.hpp"

struct EcLuauData : public Component<EntityComponent::LuauData>
{
    struct LuauComponent
    {
        std::string Name;
        struct lua_State* VM = nullptr;
        int Id = -1;
    };

    std::vector<LuauComponent> ComponentData;
    bool Valid = true;
};

class LuauDataComponentManager : public ComponentManager<EcLuauData>
{
public:
    void DeleteComponent(uint32_t Id) override;

    std::set<std::string> LuauComponentNames;
};
