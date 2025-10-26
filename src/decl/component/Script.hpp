#pragma once

#include <vector>
#include <luau/VM/src/lstate.h>

#include "datatype/GameObject.hpp"

struct EcScript
{
	bool LoadScript(const std::string_view&);
	bool Reload();
	void Update(double);

	std::string GetSource(bool* IsInline = nullptr, bool* Success = nullptr);

	std::string SourceFile = "!InlineSource:print(\"Hello, World!\")";
	ObjectRef Object;
	uint32_t EcId = UINT32_MAX;
	bool Valid = true;
	
	lua_State* m_L = nullptr;
	bool m_StaleSource = true;

	static inline EntityComponent Type = EntityComponent::Script;

	static void PushLVM(const std::string& Name);
	static void PopLVM();
};
