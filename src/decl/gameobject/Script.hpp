#pragma once


#include<vector>
#include<luau/VM/src/lstate.h>

#include"datatype/GameObject.hpp"

class Object_Script : public GameObject
{
public:
	Object_Script();

	void Initialize() override;
	void Update(double) override;

	bool LoadScript(std::string const&);
	bool Reload();

	std::string SourceFile = "scripts/empty.luau";

	PHX_GAMEOBJECT_API_REFLECTION;

	static inline bool s_WindowGrabMouse = false;

private:
	std::string m_Source;
	char* m_Bytecode;
	lua_State* m_L;
	bool m_StaleSource = false;

	bool m_HasUpdate = false;

	static void s_DeclareReflections();
	static inline Api s_Api{};
};
