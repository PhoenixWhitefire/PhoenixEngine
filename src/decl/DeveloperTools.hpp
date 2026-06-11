// 29/01/2025
// a simple toolset, build right into the engine
#pragma once

#include "script/DebugBreakReason.hpp"
#include "render/Renderer.hpp"

struct lua_State;
struct lua_Debug;

namespace DeveloperTools
{
	void Initialize(Renderer*);
	void Shutdown();

	void Frame(double DeltaTime);
	void SetExplorerSelections(const std::vector<ObjectHandle>&);
	const std::vector<ObjectHandle>& GetExplorerSelections();
	void SetExplorerRoot(const ObjectHandle);
	void OpenTextDocument(const std::string&, int Line = 1);
	// does not require `::Initialize` to be called
	void LaunchTracy();

	void DebugBreak(lua_State*, lua_Debug*, DebugBreakReason);
	void LeaveDebugger(lua_State*);

	inline bool Initialized = false;

	inline bool DocumentationShown = false;
    inline bool ExplorerShown = false;
    inline bool InfoShown = false;
    inline bool MaterialsShown = false;
    inline bool PropertiesShown = false;
    inline bool RendererShown = false;
    inline bool ScriptsShown = false;
    inline bool SettingsShown = false;
    inline bool ShadersShown = false;
};
