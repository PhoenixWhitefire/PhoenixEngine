#pragma once

class Editor
{
public:
	Editor();

	void Update(double DeltaTime);
	void RenderUI();

private:
	void m_RenderMaterialEditor();

	char* m_NewObjectClass{};
	double m_InvalidObjectErrTimeRemaining{};

	char* m_MtlCreateNameBuf{};
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
