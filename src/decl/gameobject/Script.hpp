#pragma once

#include <vector>
#include <luau/VM/src/lstate.h>

#include "datatype/GameObject.hpp"

class Object_Script : public GameObject
{
public:
	Object_Script();

	void Initialize() override;
	void Update(double) override;

	bool LoadScript(const std::string&);
	bool Reload();

	std::string SourceFile = "scripts/default.luau";

	REFLECTION_DECLAREAPI;

	static inline bool s_WindowGrabMouse = false;

private:
	std::string m_Source;
	lua_State* m_L;
	bool m_StaleSource = true;

	static void s_DeclareReflections();
	static inline Reflection::Api s_Api{};;
};
