#pragma once

#include<datatype/GameObject.hpp>
#include<gameobject/Primitive.hpp>

class Editor
{
public:
	Editor();

	void Init();
	void Update(double DeltaTime, glm::mat4 CameraTransform);
	void RenderUI();

private:

	void m_RenderMaterialEditor();

	GameObject* m_CurrentUIHierarchyRoot{};
	int m_HierarchyCurItem = -1;

	char* m_NewObjectClass{};
	double m_InvalidObjectErrTimeRemaining{};

	char* m_MtlCreateNameBuf{};
	char* m_MtlDiffuseBuf{};
	char* m_MtlSpecBuf{};

	int m_MtlCurItem = -1;
	int m_MtlPrevItem = -1;
};
