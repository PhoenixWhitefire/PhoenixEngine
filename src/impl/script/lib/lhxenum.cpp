#include <GLFW/glfw3.h>

#include "script/luhx.hpp"
#include "script/enum/keys.hpp"

int luhxopen_Enum(lua_State* L)
{
	lua_newtable(L);

	lua_newtable(L);

	for (int i = GLFW_KEY_SPACE; i <  GLFW_KEY_LAST; i++)
	{
		const char* kn = KeyNames[i];
		if (!kn)
			continue;

		lua_pushinteger(L, i);
		lua_setfield(L, -2, kn);
	}

	lua_setfield(L, -2, "Key");

	lua_newtable(L);

	lua_pushinteger(L, GLFW_MOUSE_BUTTON_LEFT);
	lua_setfield(L, -2, "Left");

	lua_pushinteger(L, GLFW_MOUSE_BUTTON_MIDDLE);
	lua_setfield(L, -2, "Middle");

	lua_pushinteger(L, GLFW_MOUSE_BUTTON_RIGHT);
	lua_setfield(L, -2, "Right");

	lua_setfield(L, -2, "MouseButton");

	lua_newtable(L);

	lua_pushinteger(L, GLFW_CURSOR_NORMAL);
	lua_setfield(L, -2, "Normal");

	lua_pushinteger(L, GLFW_CURSOR_HIDDEN);
	lua_setfield(L, -2, "Hidden");

	lua_pushinteger(L, GLFW_CURSOR_CAPTURED);
	lua_setfield(L, -2, "Captured");

	lua_pushinteger(L, GLFW_CURSOR_DISABLED);
	lua_setfield(L, -2, "Disabled");

	lua_setfield(L, -2, "CursorMode");

	lua_newtable(L);

	lua_pushinteger(L, 0);
	lua_setfield(L, -2, "None");

	lua_pushinteger(L, 1);
	lua_setfield(L, -2, "Back");

	lua_pushinteger(L, 2);
	lua_setfield(L, -2, "Front");

	lua_setfield(L, -2, "FaceCulling");

	lua_newtable(L);

	lua_pushinteger(L, 0);
	lua_setfield(L, -2, "Cube");

	lua_pushinteger(L, 1);
	lua_setfield(L, -2, "Sphere");

	lua_pushinteger(L, 2);
	lua_setfield(L, -2, "Hulls");

	lua_pushinteger(L, 3);
	lua_setfield(L, -2, "MeshComponent");

	lua_setfield(L, -2, "CollisionType");

	lua_newtable(L);

	lua_pushinteger(L, 0);
	lua_setfield(L, -2, "None");

	lua_pushinteger(L, 1);
	lua_setfield(L, -2, "Info");

	lua_pushinteger(L, 2);
	lua_setfield(L, -2, "Warning");

	lua_pushinteger(L, 3);
	lua_setfield(L, -2, "Error");

	lua_setfield(L, -2, "LogType");

	lua_newtable(L);

	lua_pushinteger(L, GLFW_RELEASE);
	lua_setfield(L, -2, "Released");

	lua_pushinteger(L, GLFW_PRESS);
	lua_setfield(L, -2, "Pressed");

	lua_pushinteger(L, GLFW_REPEAT);
	lua_setfield(L, -2, "Repeated");

	lua_setfield(L, -2, "InputAction");

	lua_newtable(L);

	// TODO: Of course, GLFW does not have a throbber cursor. Perfect.
	lua_pushinteger(L, GLFW_ARROW_CURSOR);
	lua_setfield(L, -2, "Arrow");

	lua_pushinteger(L, GLFW_IBEAM_CURSOR);
	lua_setfield(L, -2, "TextInput");

	lua_pushinteger(L, GLFW_POINTING_HAND_CURSOR);
	lua_setfield(L, -2, "PointingHand");

	lua_pushinteger(L, GLFW_CROSSHAIR_CURSOR);
	lua_setfield(L, -2, "Crosshair");

	lua_pushinteger(L, GLFW_RESIZE_EW_CURSOR);
	lua_setfield(L, -2, "ResizeHorizontal");

	lua_pushinteger(L, GLFW_RESIZE_NS_CURSOR);
	lua_setfield(L, -2, "ResizeVertical");

	lua_pushinteger(L, GLFW_RESIZE_NWSE_CURSOR);
	lua_setfield(L, -2, "ResizeNWSE");

	lua_pushinteger(L, GLFW_RESIZE_NESW_CURSOR);
	lua_setfield(L, -2, "ResizeNESW");

	lua_pushinteger(L, GLFW_RESIZE_ALL_CURSOR);
	lua_setfield(L, -2, "ResizeAll");

	lua_pushinteger(L, GLFW_NOT_ALLOWED_CURSOR);
	lua_setfield(L, -2, "NotAllowed");

	lua_setfield(L, -2, "Cursor");

	lua_newtable(L);

	for (int i = Reflection::ValueType::Boolean; i < Reflection::ValueType::__lastBase; i++)
	{
		lua_pushinteger(L, i);
		lua_setfield(L, -2, Reflection::TypeAsString((Reflection::ValueType)i).c_str());
	}

	lua_pushinteger(L, Reflection::ValueType::Any);
	lua_setfield(L, -2, "Any");
	lua_pushinteger(L, Reflection::ValueType::Null);
	lua_setfield(L, -2, "Null");

	lua_setfield(L, -2, "ValueType");

	lua_setglobal(L, "Enum");

    return 1;
}
