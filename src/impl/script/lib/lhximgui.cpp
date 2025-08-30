#include <imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include "script/luhx.hpp"
#include "asset/TextureManager.hpp"
#include "Engine.hpp"

static int imgui_begin(lua_State* L)
{
    const char* title = luaL_checkstring(L, 1);
	lua_pushboolean(L, ImGui::Begin(title));

	return 1;
}

static int imgui_end(lua_State* L)
{
    ImGui::End();

    return 0;
}

static int imgui_indent(lua_State* L)
{
    ImGui::Indent(static_cast<float>(luaL_checknumber(L, 1)));

    return 0;
}

static int imgui_setitemtooltip(lua_State* L)
{
    ImGui::SetItemTooltip("%s", luaL_checkstring(L, 1));

	return 0;
}

static int imgui_itemhovered(lua_State* L)
{
    lua_pushboolean(L, ImGui::IsItemHovered());

	return 1;
}

static int imgui_itemclicked(lua_State* L)
{
    lua_pushboolean(L, ImGui::IsItemClicked());

	return 1;
}

static int imgui_text(lua_State* L)
{
    ImGui::TextUnformatted(luaL_checkstring(L, 1));

	return 0;
}

static int imgui_image(lua_State* L)
{
    TextureManager* texManager = TextureManager::Get();
	uint32_t resId = texManager->LoadTextureFromPath(luaL_checkstring(L, 1));
	const Texture& texture = texManager->GetTextureResource(resId);

	ImGui::Image(
		texture.GpuId,
		ImVec2(
			static_cast<float>(luaL_optnumber(L, 2, texture.Width)),
			static_cast<float>(luaL_optnumber(L, 3, texture.Height))
		)
	);

    return 0;
}

static int imgui_inputstring(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
	std::string value = luaL_checkstring(L, 2);
    value.reserve(value.size() + 64);
	ImGui::InputText(name, &value);

	lua_pushstring(L, value.c_str());
	return 1;
}

static int imgui_inputnumber(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
	double value = luaL_checknumber(L, 2);
	ImGui::InputDouble(name, &value);

	lua_pushnumber(L, value);
	return 1;
}

static int imgui_button(lua_State* L)
{
    lua_pushboolean(L, ImGui::Button(luaL_checkstring(L, 1)));

    return 1;
}

static int imgui_textlink(lua_State* L)
{
    lua_pushboolean(L, ImGui::TextLink(luaL_checkstring(L, 1)));

    return 1;
}

static int imgui_checkbox(lua_State* L)
{
    const char* title = luaL_checkstring(L, 1);
	bool curval = luaL_checkboolean(L, 2);
	bool pressed = ImGui::Checkbox(title, &curval);

	lua_pushboolean(L, curval);
	lua_pushboolean(L, pressed);

    return 2;
}

static luaL_Reg imgui_funcs[] =
{
    { "begin", imgui_begin },
    { "endw", imgui_end },
    { "indent", imgui_indent },
    { "setitemtooltip", imgui_setitemtooltip },
    { "itemhovered", imgui_itemhovered },
    { "itemclicked", imgui_itemclicked },
    { "text", imgui_text },
    { "image", imgui_image },
    { "inputnumber", imgui_inputstring },
    { "inputstring", imgui_inputnumber },
    { "button", imgui_button },
    { "textlink", imgui_textlink },
    { "checkbox", imgui_checkbox },
    { NULL, NULL }
};

int luhxopen_imgui(lua_State* L)
{
    luaL_register(L, LUHX_IMGUILIBNAME, imgui_funcs);

    return 1;
}
