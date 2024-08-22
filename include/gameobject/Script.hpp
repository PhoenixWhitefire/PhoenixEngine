#pragma once

/*
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

private:

	std::string m_source;
	char* m_bytecode;
	lua_State* L;

	bool hasUpdate = false;

	static RegisterDerivedObject<Object_Script> RegisterClassAs;
	static void s_DeclareReflections();
};
*/