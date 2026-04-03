#pragma once

#include "datatype/ComponentBase.hpp"

struct EcModel : public Component<EntityComponent::Model>
{
	std::string ImportPath;
	bool Valid = true;
};
