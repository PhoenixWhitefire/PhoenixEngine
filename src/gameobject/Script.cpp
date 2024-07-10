#include<lua.h>
#include<luacode.h>

#include"gameobject/Script.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"

DerivedObjectRegister<Object_Script> Object_Script::RegisterClassAs("Script");

static void* l_alloc(void* ud, void* ptr, size_t osize,
	size_t nsize) {
	(void)ud;  (void)osize;  /* not used */
	if (nsize == 0) {
		free(ptr);
		return NULL;
	}
	else
		return realloc(ptr, nsize);
}

static lua_State* L = lua_newstate(l_alloc, nullptr);

Object_Script::Object_Script()
{
	this->Name = "Script";
	this->ClassName = "Script";
	this->m_bytecode = NULL;

	m_properties.insert(std::pair(
		"SourceFile",
		PropInfo
		{
			PropType::String,
			PropReflection
			{
				[this]() { return GenericType{ PropType::String, this->SourceFile }; },
				[this](GenericType gt) { this->SourceFile = gt.String; }
			}
		}
	));

	m_procedures.insert(std::pair(
		"Reload",
		[this]() { this->Reload(); }
	));
}

void Object_Script::Initialize()
{
	this->Reload();
}

bool Object_Script::LoadScript(std::string scriptFile)
{
	if (SourceFile == scriptFile)
		return true;

	this->SourceFile = scriptFile;

	return this->Reload();
}

bool Object_Script::Reload()
{
	bool fileExists = true;
	m_source = FileRW::ReadFile(SourceFile, &fileExists);

	if (!fileExists)
	{
		Debug::Log(
			std::vformat(
				"Script '{}' references invalid Source File '{}'!",
				std::make_format_args(this->Name, this->SourceFile
				)
			)
		);

		return false;
	}

	std::string chunkname = std::vformat("Luau Chunk for Script {}", std::make_format_args(this->Name));

	// FROM: https://github.com/luau-lang/luau/ README

	// needs lua.h and luacode.h
	size_t bytecodeSize = 0;
	this->m_bytecode = luau_compile(m_source.c_str(), m_source.length(), NULL, &bytecodeSize);

	int result = luau_load(L, chunkname.c_str(), m_bytecode, bytecodeSize, 0);
	
	printf("%i\n", result);

	if (result == 0)
		return true; /* return chunk main function */
	else
		return false;
}
