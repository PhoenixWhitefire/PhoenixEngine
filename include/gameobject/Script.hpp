#pragma once

#include<vector>
#include<lua.h>
#include<luacode.h>

#include"datatype/GameObject.hpp"

class Object_Script : public GameObject
{
public:

	Object_Script();

	void Initialize();
	void Update(double);

	bool LoadScript(std::string scriptFile);
	bool Reload();

	std::string SourceFile = "scripts/default.luau";

	std::vector<std::shared_ptr<GameObject>> newobjs;

private:

	std::string m_source;
	char* m_bytecode;
	lua_State* L;

	bool hasUpdate = false;

	static DerivedObjectRegister<Object_Script> RegisterClassAs;
};
