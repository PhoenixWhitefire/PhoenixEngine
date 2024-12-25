#pragma once

#include <SDL2/SDL_video.h>

#include "render/Renderer.hpp"

class Editor
{
public:
	Editor(Renderer*);

	void Update(double DeltaTime);
	void RenderUI();

private:
	void m_RenderMaterialEditor();

	char* m_MtlCreateNameBuf{};
	char* m_MtlLoadNameBuf{};
	char* m_MtlDiffuseBuf{};
	char* m_MtlSpecBuf{};
	char* m_MtlNormalBuf{};
	char* m_MtlEmissionBuf{};
	char* m_MtlShpBuf{};
	char* m_MtlNewUniformNameBuf{};
	char* m_MtlUniformNameEditBuf{};

	int m_MtlCurItem = -1;
	int m_MtlPrevItem = -1;
};
