// The Hierarchy Root
// 13/08/2024

#pragma once

#include "component/Workspace.hpp"
#include "Reflection.hpp"

struct EcDataModel : public Component<EntityComponent::DataModel>
{
	uint32_t Workspace = UINT32_MAX;
	ObjectRef Object;

	std::vector<Reflection::EventCallback> OnFrameBeginCallbacks;
	bool Valid = true;
};
