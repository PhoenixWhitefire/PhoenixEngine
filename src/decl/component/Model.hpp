#pragma once

#include "datatype/ComponentBase.hpp"

struct EcModel : public Component<EntityComponent::Model>
{
	bool Valid = true;
};
