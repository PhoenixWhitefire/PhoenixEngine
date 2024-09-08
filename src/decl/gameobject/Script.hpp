#pragma once


#include<vector>
#include<luau/VM/src/lstate.h>

#include"datatype/GameObject.hpp"

class Object_Script : public GameObject
{
public:

	Object_Script();

	void Initialize();
	void Update(double);

	bool LoadScript(std::string const& scriptFile);
	bool Reload();

	std::string SourceFile = "scripts/empty.luau";

	PHX_GAMEOBJECT_API_REFLECTION;

private:
	std::string m_Source;
	char* m_Bytecode;
	lua_State* m_L;

	bool m_HasUpdate = false;

	static void s_DeclareReflections();
	static inline Api s_Api{};
};
