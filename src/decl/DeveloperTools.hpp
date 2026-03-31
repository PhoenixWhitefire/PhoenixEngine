// 29/01/2025
// a simple toolset, build right into the engine
#pragma once

#include "render/Renderer.hpp"

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

	inline bool Initialized = false;

	inline bool DocumentationShown = false;
    inline bool ExplorerShown = true;
    inline bool InfoShown = false;
    inline bool MaterialsShown = false;
    inline bool PropertiesShown = true;
    inline bool RendererShown = false;
    inline bool ScriptsShown = true;
    inline bool SettingsShown = false;
    inline bool ShadersShown = false;
};
