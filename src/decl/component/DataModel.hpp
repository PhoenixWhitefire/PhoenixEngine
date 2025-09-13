// The Hierarchy Root
// 13/08/2024

#pragma once

#include "component/Workspace.hpp"
#include "Reflection.hpp"

struct EcDataModel
{
	uint32_t Workspace = UINT32_MAX;

	std::vector<Reflection::EventCallback> OnFrameBeginCallbacks;
	bool Valid = true;

	static inline EntityComponent Type = EntityComponent::DataModel;
};
