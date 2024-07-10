#pragma once

#include"datatype/GameObject.hpp"

class Object_Script : public GameObject
{
public:

	Object_Script();

	void Initialize();

	bool LoadScript(std::string scriptFile);
	bool Reload();

	std::string SourceFile = "scripts/default.luau";

private:

	std::string m_source;
	char* m_bytecode;

	static DerivedObjectRegister<Object_Script> RegisterClassAs;
};
