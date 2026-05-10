// ScriptEngine service, 10/05/2026
#pragma once

#include "datatype/ComponentBase.hpp"

struct EcScriptEngineService : Component<EntityComponent::ScriptEngine>
{
    bool Valid = true;
};

class ScriptEngineComponentManager : public ComponentManager<EcScriptEngineService>
{
    const Reflection::StaticMethodMap& GetMethods() override;
};
