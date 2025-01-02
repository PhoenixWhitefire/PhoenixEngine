#include "gameobject/Animation.hpp"

PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(Animation);

static bool s_DidInitReflections = false;

void Object_Animation::s_DeclareReflections()
{
	if (s_DidInitReflections)
		return;
	s_DidInitReflections = true;
}

Object_Animation::Object_Animation()
{
	this->ClassName = "Animation";

	s_DeclareReflections();
	ApiPointer = &s_Api;
}
