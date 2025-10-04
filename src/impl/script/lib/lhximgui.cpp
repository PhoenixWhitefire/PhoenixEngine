#define IMGUI_DEFINE_MATH_OPERATORS

#include <imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include "script/luhx.hpp"
#include "asset/TextureManager.hpp"
#include "Engine.hpp"

static ImGuiWindowFlags strToWindowFlags(lua_State* L, const char* flagsstr)
{
    ImGuiWindowFlags flags = 0;

    for (; *flagsstr; flagsstr++)
    {
        switch (*flagsstr)
        {
        case 'n':
        {
            flagsstr++;

            switch (*flagsstr)
            {
            case 't':
            {
                flags |= ImGuiWindowFlags_NoTitleBar;
                break;
            }
            case 'r':
            {
                flags |= ImGuiWindowFlags_NoResize;
                break;
            }
            case 'm':
            {
                flags |= ImGuiWindowFlags_NoMove;
                break;
            }
            case 'c':
            {
                flags |= ImGuiWindowFlags_NoCollapse;
                break;
            }
            case 's':
            {
                flags |= ImGuiWindowFlags_NoSavedSettings;
                break;
            }
            case 'i':
            {
                flags |= ImGuiWindowFlags_NoInputs;
                break;
            }

            default:
            {
                luaL_error(L, "Invalid option to flag 'n': '%c'", *flagsstr);
                break;
            }
            }

            break;
        }

        case 'h':
        {
            flags |= ImGuiWindowFlags_HorizontalScrollbar;
            break;
        }

        case ' ': case '|':
            break;

        default:
        {
            luaL_error(L, "Unknown flag '%c'", *flagsstr);
        }
        }
    }

    return flags;
}

static int imgui_begin(lua_State* L)
{
    const char* title = luaL_checkstring(L, 1);
    const char* flagsstr = luaL_optstring(L, 2, "");

	lua_pushboolean(L, ImGui::Begin(title, nullptr, strToWindowFlags(L, flagsstr)));

	return 1;
}

static int imgui_end(lua_State*)
{
    ImGui::End();

    return 0;
}

static int imgui_indent(lua_State* L)
{
    ImGui::Indent(static_cast<float>(luaL_optnumber(L, 1, 0.f)));

    return 0;
}

static int imgui_setitemtooltip(lua_State* L)
{
    ImGui::SetItemTooltip("%s", luaL_checkstring(L, 1));

	return 0;
}

static int imgui_settooltip(lua_State* L)
{
    ImGui::SetTooltip("%s", luaL_checkstring(L, 1));

    return 0;
}

static int imgui_itemhovered(lua_State* L)
{
    lua_pushboolean(L, ImGui::IsItemHovered());

	return 1;
}

static int imgui_anyitemactive(lua_State* L)
{
    lua_pushboolean(L, ImGui::IsAnyItemActive());

    return 1;
}

static int imgui_windowhovered(lua_State* L)
{
    lua_pushboolean(L, ImGui::IsWindowHovered());

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

    ImVec2 texSize = ImVec2(texture.Width, texture.Height);
    ImVec2 uv0 = ImVec2(0.f, luaL_optboolean(L, 3, false) ? 1.f : 0.f);
    ImVec2 uv1 = ImVec2(1.f, luaL_optboolean(L, 3, false) ? 0.f : 1.f);

    if (!lua_isnoneornil(L, 2))
    {
        luaL_checktype(L, 2, LUA_TTABLE);
        
        lua_pushinteger(L, 1);
        if (lua_gettable(L, 2) != LUA_TNIL)
            texSize.x = luaL_checknumber(L, -1);

        lua_pushinteger(L, 2);
        if (lua_gettable(L, 2) != LUA_TNIL)
            texSize.y = luaL_checknumber(L, -1);

        lua_pop(L, 2);
    }

    ImVec4 tintcol = ImVec4(1.f, 1.f, 1.f, 1.f);

    if (!lua_isnoneornil(L, 4))
    {
        luaL_checktype(L, 4, LUA_TTABLE);

        lua_pushinteger(L, 1);
        if (lua_gettable(L, 4) != LUA_TNIL)
            tintcol.x = luaL_checknumber(L, -1);

        lua_pushinteger(L, 2);
        if (lua_gettable(L, 4) != LUA_TNIL)
            tintcol.y = luaL_checknumber(L, -1);

        lua_pushinteger(L, 3);
        if (lua_gettable(L, 4) != LUA_TNIL)
            tintcol.z = luaL_checknumber(L, -1);

        lua_pushinteger(L, 4);
        if (lua_gettable(L, 4) != LUA_TNIL)
            tintcol.w = luaL_checknumber(L, -1);
    }

	ImGui::ImageWithBg(
		texture.GpuId,
		texSize,
        uv0, uv1,
        ImVec4(),
        tintcol
	);

    return 0;
}

static int imgui_imagebutton(lua_State* L)
{
    TextureManager* texManager = TextureManager::Get();
	uint32_t resId = texManager->LoadTextureFromPath(luaL_checkstring(L, 2));
	const Texture& texture = texManager->GetTextureResource(resId);

    ImVec2 imgSize = ImVec2(texture.Width, texture.Height);

    if (!lua_isnoneornil(L, 3))
    {
        luaL_checktype(L, 3, LUA_TTABLE);
        
        lua_pushinteger(L, 1);
        if (lua_gettable(L, 3) != LUA_TNIL)
            imgSize.x = luaL_checknumber(L, -1);

        lua_pushinteger(L, 2);
        if (lua_gettable(L, 3) != LUA_TNIL)
            imgSize.y = luaL_checknumber(L, -1);

        lua_pop(L, 2);
    }

    lua_pushboolean(L, ImGui::ImageButton(
        luaL_checkstring(L, 1),
        texture.GpuId,
        imgSize
    ));

    return 1;
}

static int imgui_inputstring(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
	std::string value = luaL_checkstring(L, 2);
    value.reserve(value.size() + 64);
	ImGui::InputText(name, &value);

	lua_pushlstring(L, value.data(), value.size());
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
    lua_pushboolean(L, ImGui::Button(luaL_checkstring(L, 1), { (float)luaL_optnumber(L, 2, 0.f), (float)luaL_optnumber(L, 3, 0.f) }));

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

static int imgui_stylecolors(lua_State* L)
{
    size_t siz = 0;
    const char* n = luaL_checklstring(L, 1, &siz);

    if (siz == 1)
    {
        if (n[0] == 'L')
        {
            ImGui::StyleColorsLight();
            return 0;
        }
        else if (n[0] == 'D')
        {
            ImGui::StyleColorsDark();
            return 0;
        }
        else
            luaL_error(L, "Invalid style '%s'", n);
    }
    
    luaL_error(L, "Invalid style '%s'", n);
}

static int imgui_setnextwindowfocus(lua_State*)
{
    ImGui::SetNextWindowFocus();

    return 0;
}

static int imgui_setnextwindowposition(lua_State* L)
{
    ImGui::SetNextWindowPos({ (float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2) });

    return 0;
}

static int imgui_setnextwindowsize(lua_State* L)
{
    ImGui::SetNextWindowSize({ (float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2) });

    return 0;
}

static int imgui_beginfullscreen(lua_State* L)
{
    ImVec2 offset( luaL_optnumber(L, 2, 0.f), luaL_optnumber(L, 3, 0.f) );

    ImGuiIO& guiIO = ImGui::GetIO();
    ImGui::SetNextWindowPos(offset);
    ImGui::SetNextWindowSize(guiIO.DisplaySize - offset);

    lua_pushboolean(L, ImGui::Begin(
        luaL_optstring(L, 1, "FullscreenWindow"),
        nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
    ));
    return 1;
}

static int imgui_beginmainmenubar(lua_State* L)
{
    lua_pushboolean(L, ImGui::BeginMainMenuBar());

    return 1;
}

static int imgui_endmainmenubar(lua_State*)
{
    ImGui::EndMainMenuBar();

    return 0;
}

static int imgui_beginmenubar(lua_State* L)
{
    lua_pushboolean(L, ImGui::BeginMenuBar());

    return 1;
}

static int imgui_endmenubar(lua_State*)
{
    ImGui::EndMenuBar();

    return 0;
}

static int imgui_beginmenu(lua_State* L)
{
    lua_pushboolean(L, ImGui::BeginMenu(luaL_checkstring(L, 1), luaL_optboolean(L, 2, true)));

    return 1;
}

static int imgui_endmenu(lua_State*)
{
    ImGui::EndMenu();

    return 0;
}

static int imgui_menuitem(lua_State* L)
{
    lua_pushboolean(L, ImGui::MenuItem(luaL_checkstring(L, 1), nullptr, nullptr, luaL_optboolean(L, 2, true)));

    return 1;
}

static int imgui_getcursorpos(lua_State* L)
{
    ImVec2 cpos = ImGui::GetCursorPos();

    lua_pushnumber(L, cpos.x);
    lua_pushnumber(L, cpos.y);
    return 2;
}

static int imgui_setcursorpos(lua_State* L)
{
    ImGui::SetCursorPos({ (float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2) });

    return 0;
}

static int imgui_dummy(lua_State* L)
{
    ImGui::Dummy({ (float)luaL_optnumber(L, 1, 0.f), (float)luaL_optnumber(L, 2, 0.f) });

    return 0;
}

static int imgui_beginchild(lua_State* L)
{
    ImGuiWindowFlags winFlags = strToWindowFlags(L, luaL_optstring(L, 5, ""));
    ImGuiChildFlags flags = 0;
    const char* flagsstr = luaL_optstring(L, 4, "");

    for (; *flagsstr; flagsstr++)
    {
        switch (*flagsstr)
        {

        case 'b':
        {
            flags |= ImGuiChildFlags_Borders;
            break;
        }

        case 'w':
        {
            if (*(flagsstr+1) == 'p')
                flags |= ImGuiChildFlags_AlwaysUseWindowPadding;
            else
                luaL_error(L, "Expected 'wp', got 'w%c'", *(flagsstr+1));

            flagsstr++;
            break;
        }

        case 'r':
        {
            if (*(flagsstr+1) == 'x')
                flags |= ImGuiChildFlags_ResizeX;

            else if (*(flagsstr+1) == 'y')
                flags |= ImGuiChildFlags_ResizeY;

            else
                luaL_error(L, "Expected next character to be 'x' or 'y', got '%c'", *(flagsstr+1));

            flagsstr++;
            break;
        }

        case 'a':
        {
            if (*(flagsstr+1) == 'x')
                flags |= ImGuiChildFlags_AutoResizeX;

            else if (*(flagsstr+1) == 'y')
                flags |= ImGuiChildFlags_AutoResizeY;

            else
                flags |= ImGuiChildFlags_AlwaysAutoResize;

            flagsstr++;
            break;
        }

        case 'f':
        {
            flags |= ImGuiChildFlags_FrameStyle;
            break;
        }

        case ' ': case '|':
        {
            break;
        }

        default:
        {
            luaL_error(L, "Invalid flag '%c'", *flagsstr);
        }

        }
    }

    lua_pushboolean(
        L,
        ImGui::BeginChild(
            luaL_checkstring(L, 1),
            { (float)luaL_optnumber(L, 2, 0.f), (float)luaL_optnumber(L, 3, 0.f) },
            flags, winFlags
        )
    );

    return 1;
}

static int imgui_endchild(lua_State*)
{
    ImGui::EndChild();

    return 0;
}

static int imgui_combo(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TTABLE);
    int curopt = luaL_checkinteger(L, 3) - 1;

    std::string options;
    options.reserve(lua_objlen(L, 2));

    lua_pushnil(L);
    while (lua_next(L, 2))
    {
        luaL_checktype(L, -2, LUA_TNUMBER);
        options.append(luaL_checkstring(L, -1));
        options.push_back('\0');

        lua_pop(L, 2);
    }

    options.push_back('\0');

    ImGui::Combo(luaL_checkstring(L, 1), &curopt, options.data());

    lua_pushinteger(L, curopt + 1);
    return 1;
}

static int imgui_treenode(lua_State* L)
{
    lua_pushboolean(L, ImGui::TreeNode(luaL_checkstring(L, 1)));
    return 1;
}

static int imgui_treepop(lua_State* L)
{
    ImGui::TreePop();
    return 0;
}

static int imgui_pushid(lua_State* L)
{
    ImGui::PushID(luaL_checkstring(L, 1));
    return 0;
}

static int imgui_popid(lua_State* L)
{
    ImGui::PopID();
    return 0;
}

static int imgui_sameline(lua_State* L)
{
    ImGui::SameLine();
    return 0;
}

static int imgui_setnextwindowopen(lua_State* L)
{
    ImGui::SetNextWindowCollapsed(!luaL_optboolean(L, 1, true));
    return 0;
}

static int imgui_getcontentregionavail(lua_State* L)
{
    ImVec2 avail = ImGui::GetContentRegionAvail();

    lua_pushnumber(L, avail.x);
    lua_pushnumber(L, avail.y);
    return 2;
}

static int imgui_separator(lua_State*)
{
    ImGui::Separator();
    return 0;
}

static int imgui_separatortext(lua_State* L)
{
    ImGui::SeparatorText(luaL_checkstring(L, 1));
    return 0;
}

static int imgui_pushstylecolor(lua_State* L)
{
    lua_pushinteger(L, 1);
    lua_gettable(L, 2);
    lua_pushinteger(L, 2);
    lua_gettable(L, 2);
    lua_pushinteger(L, 3);
    lua_gettable(L, 2);
    lua_pushinteger(L, 4);
    lua_gettable(L, 2);

    ImVec4 col;
    col.x = luaL_checknumber(L, -4);
    col.y = luaL_checknumber(L, -3);
    col.z = luaL_checknumber(L, -2);
    col.w = luaL_checknumber(L, -1);

    ImGui::PushStyleColor(luaL_checkinteger(L, 1), col);

    return 0;
}

static int imgui_popstylecolor(lua_State*)
{
    ImGui::PopStyleColor();

    return 0;
}

static int imgui_getwindowposition(lua_State* L)
{
    ImVec2 pos = ImGui::GetWindowPos();

    lua_pushnumber(L, pos.x);
    lua_pushnumber(L, pos.y);
    return 2;
}

static int imgui_getwindowsize(lua_State* L)
{
    ImVec2 size = ImGui::GetWindowSize();

    lua_pushnumber(L, size.x);
    lua_pushnumber(L, size.y);
    return 2;
}

static luaL_Reg imgui_funcs[] =
{
    { "begin", imgui_begin },
    { "endw", imgui_end },
    { "indent", imgui_indent },
    { "setitemtooltip", imgui_setitemtooltip },
    { "settooltip", imgui_settooltip },
    { "itemhovered", imgui_itemhovered },
    { "itemclicked", imgui_itemclicked },
    { "anyitemactive", imgui_anyitemactive },
    { "windowhovered", imgui_windowhovered },
    { "text", imgui_text },
    { "image", imgui_image },
    { "imagebutton", imgui_imagebutton },
    { "inputnumber", imgui_inputnumber },
    { "inputstring", imgui_inputstring },
    { "button", imgui_button },
    { "textlink", imgui_textlink },
    { "checkbox", imgui_checkbox },
    { "stylecolors", imgui_stylecolors },
    { "setnextwindowfocus", imgui_setnextwindowfocus },
    { "setnextwindowposition", imgui_setnextwindowposition },
    { "setnextwindowsize", imgui_setnextwindowsize },
    { "beginfullscreen", imgui_beginfullscreen },
    { "beginmainmenubar", imgui_beginmainmenubar },
    { "endmainmenubar", imgui_endmainmenubar },
    { "beginmenubar", imgui_beginmenubar },
    { "endmenubar", imgui_endmenubar },
    { "beginmenu", imgui_beginmenu },
    { "endmenu", imgui_endmenu },
    { "menuitem", imgui_menuitem },
    { "getcursorpos", imgui_getcursorpos },
    { "setcursorpos", imgui_setcursorpos },
    { "dummy", imgui_dummy },
    { "beginchild", imgui_beginchild },
    { "endchild", imgui_endchild },
    { "combo", imgui_combo },
    { "treenode", imgui_treenode },
    { "treepop", imgui_treepop },
    { "pushid", imgui_pushid },
    { "popid", imgui_popid },
    { "sameline", imgui_sameline },
    { "setnextwindowopen", imgui_setnextwindowopen },
    { "getcontentregionavail", imgui_getcontentregionavail },
    { "separator", imgui_separator },
    { "separatortext", imgui_separatortext },
    { "pushstylecolor", imgui_pushstylecolor },
    { "popstylecolor", imgui_popstylecolor },
    { "getwindowposition", imgui_getwindowposition },
    { "getwindowsize", imgui_getwindowsize },
    { NULL, NULL }
};

int luhxopen_imgui(lua_State* L)
{
    if (Engine::Get()->IsHeadlessMode)
    {
        luaL_Reg* l = &imgui_funcs[0];
        for (; l->name; l++)
        {
            l->func = [](lua_State* L)
                -> int
                {
                    luaL_error(L, "Function cannot be called in headless mode");
                };
        }
    }

    luaL_register(L, LUHX_IMGUILIBNAME, imgui_funcs);

    return 1;
}
