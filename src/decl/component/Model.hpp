#pragma once

#include "datatype/GameObject.hpp"

struct EcModel : public Component<EntityComponent::Model>
{
	bool Valid = true;
};
