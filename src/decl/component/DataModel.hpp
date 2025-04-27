// The Hierarchy Root
// 13/08/2024

#pragma once

#include "component/Workspace.hpp"

struct EcDataModel
{
	uint32_t Workspace = UINT32_MAX;

	static inline EntityComponent Type = EntityComponent::DataModel;
};
