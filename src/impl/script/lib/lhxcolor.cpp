#include "script/luhx.hpp"
#include "datatype/Color.hpp"

void luhx_pushcolor(lua_State* L, const Color& col)
{
	Color* ptr = (Color*)lua_newuserdata(L, sizeof(Color));
	*ptr = col;

	luaL_getmetatable(L, LUHX_COLORLIBNAME);
	lua_setmetatable(L, -2);
}

static int color_new(lua_State* L)
{
    int argType = lua_type(L, 1);
    luaL_argcheck(L, argType == LUA_TNUMBER || argType== LUA_TVECTOR, 1, "expected number or vector");

    Color col;

    if (argType == LUA_TVECTOR)
	{
		const float* v = lua_tovector(L, 1);
		col.R = v[0];
		col.G = v[1];
		col.B = v[2];
	}
    else
    {
        col.R = luaL_checknumber(L, 1);
        col.G = luaL_checknumber(L, 2);
        col.B = luaL_checknumber(L, 3);
    }

	luhx_pushcolor(L, col);
	return 1;
}

static luaL_Reg color_funcs[] =
{
	{ "new", color_new },
	{ NULL, NULL }
};

static int col_index(lua_State* L)
{
    Color* vec = (Color*)luaL_checkudata(L, 1, LUHX_COLORLIBNAME);
	size_t ksize = 0;
	const char* key = luaL_checklstring(L, 2, &ksize);

	if (ksize == 1)
	{
		switch (key[0])
		{
		case 'R':
			lua_pushnumber(L, vec->R);
			break;
		case 'G':
			lua_pushnumber(L, vec->G);
			break;
		case 'B':
			lua_pushnumber(L, vec->B);
			break;
		default:
			luaL_error(L, "'%s' is not a valid member of Colors", key);
		}

		return 1;
	}

	luaL_error(L, "'%s' is not a valid member of Colors", key);
}

static int col_tostring(lua_State* L)
{
	Color* col = (Color*)luaL_checkudata(L, 1, LUHX_COLORLIBNAME);

	lua_pushfstringL(L, "%.2f, %.2f, %.2f", col->R, col->G, col->B);
	return 1;
};

static int col_eq(lua_State* L)
{
	Color* a = (Color*)luaL_checkudata(L, 1, LUHX_COLORLIBNAME);
	Color* b = (Color*)luaL_checkudata(L, 2, LUHX_COLORLIBNAME);

	lua_pushboolean(L, a->R == b->R && a->G == b->G && a->B == b->B);
	return 1;
}

static void createmetatable(lua_State* L)
{
    luaL_newmetatable(L, LUHX_COLORLIBNAME);

	lua_pushcfunction(L, col_index, "Color.__index");
	lua_setfield(L, -2, "__index");

	lua_pushcfunction(L, col_tostring, "Color.__tostring");
	lua_setfield(L, -2, "__tostring");

	lua_pushcfunction(L, col_eq, "Color.__eq");
	lua_setfield(L, -2, "__eq");

	lua_pushstring(L, LUHX_COLORLIBNAME);
	lua_setfield(L, -2, "__type");

	lua_pop(L, 1);
}

int luhxopen_Color(lua_State* L)
{
    luaL_register(L, LUHX_COLORLIBNAME, color_funcs);
    createmetatable(L);

    return 1;
}
