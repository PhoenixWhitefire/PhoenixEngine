// 29/01/2025
// a simple toolset, build right into the engine

#pragma once

#include "render/Renderer.hpp"

namespace InlayEditor
{
	void Initialize(Renderer*);

	void UpdateAndRender(double DeltaTime);
	void SetExplorerSelections(const std::vector<ObjectHandle>&);
	const std::vector<ObjectHandle>& GetExplorerSelections();
	void SetExplorerRoot(const ObjectHandle);
	void OpenTextDocument(const std::string&, int Line = 1);

	void Shutdown();

	extern bool DidInitialize;
};
