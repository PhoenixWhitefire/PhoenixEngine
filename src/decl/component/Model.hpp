#pragma once

#include "datatype/ComponentBase.hpp"

struct EcModel : public Component<EntityComponent::Model>
{
	std::string ImportPath;
	bool Valid = true;
};

class ModelComponentManager : public ComponentManager<EcModel>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override;
};
