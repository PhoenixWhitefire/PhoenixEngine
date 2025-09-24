#pragma once

#include <vector>
#include <luau/VM/src/lstate.h>

#include "datatype/GameObject.hpp"

struct EcScript
{
	bool LoadScript(const std::string_view&);
	bool Reload();
	void Update(double);

	std::string SourceFile = "scripts/default.luau";
	ObjectRef Object;
	uint32_t EcId = UINT32_MAX;
	bool Valid = true;

	std::string m_Source;
	lua_State* m_L = nullptr;
	bool m_StaleSource = true;

	static inline EntityComponent Type = EntityComponent::Script;

	static void PushLVM(const std::string& Name);
	static void PopLVM();
};
