#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <cstring>

#include "script/luhx.hpp"

void luhx_pushmatrix(lua_State* L, const glm::mat4& mtx)
{
    void* ptr = lua_newuserdata(L, sizeof(glm::mat4));
    memcpy(ptr, &mtx, sizeof(glm::mat4));

    luaL_getmetatable(L, LUHX_MATRIXLIBNAME);
    lua_setmetatable(L, -2);
}

static int matrix_new(lua_State* L)
{
    glm::mat4 matrix = glm::mat4(1.f);

    luhx_pushmatrix(L, matrix);
	return 1;
}

static int matrix_fromTranslation(lua_State* L)
{
    glm::mat4 m = glm::mat4(1.f);

	int numArgs = lua_gettop(L);

	switch (numArgs)
	{
	case 1:
	{
		const float* vec = luaL_checkvector(L, -1);
		m[3] = glm::vec4(glm::make_vec3(vec), 1.f);

		break;
	}
	case 3:
	{
		float x = static_cast<float>(luaL_checknumber(L, 1));
		float y = static_cast<float>(luaL_checknumber(L, 2));
		float z = static_cast<float>(luaL_checknumber(L, 3));

	    m[3] = glm::vec4(glm::vec3(x, y, z), 1.f);

		break;
	}

	default:
		luaL_error(
			L,
			"`Matrix.fromTranslation` expected 1 or 3 arguments, got %i",
			numArgs
		);
	}

    luhx_pushmatrix(L, m);
    return 1;
}

static int matrix_fromEulerAnglesXYZ(lua_State* L)
{
    glm::vec3 ang;

    if (const float* v = lua_tovector(L, 1))
        ang = glm::make_vec3(v);

    else
    {
        ang.x = static_cast<float>(luaL_checknumber(L, 1));
        ang.y = static_cast<float>(luaL_checknumber(L, 2));
        ang.z = static_cast<float>(luaL_checknumber(L, 3));
    }

	glm::mat4 m = glm::mat4(1.f);
	m = glm::rotate(m, ang.x, glm::vec3(1.f, 0.f, 0.f));
	m = glm::rotate(m, ang.y, glm::vec3(0.f, 1.f, 0.f));
	m = glm::rotate(m, ang.z, glm::vec3(0.f, 0.f, 1.f));

	luhx_pushmatrix(L, m);
	return 1;
}

static int matrix_lookAt(lua_State* L)
{
    const float updefault[3] = { 0.f, 1.f, 0.f };

    const float* a = luaL_checkvector(L, 1);
	const float* b = luaL_checkvector(L, 2);
    const float* up = luaL_optvector(L, 3, updefault);

	luhx_pushmatrix(
		L,
		glm::lookAt(glm::make_vec3(a), glm::make_vec3(b), glm::make_vec3(up))
	);
	return 1;
}

static const luaL_Reg matrix_funcs[] =
{
	{ "new", matrix_new },
    { "fromTranslation", matrix_fromTranslation },
    { "fromEulerAnglesXYZ", matrix_fromEulerAnglesXYZ },
    { "lookAt", matrix_lookAt },

	{ NULL, NULL }
};

static int mtx_index(lua_State* L)
{
    glm::mat4& m = *(glm::mat4*)luaL_checkudata(L, 1, LUHX_MATRIXLIBNAME);
	size_t klen = 0;
	const char* k = luaL_checklstring(L, 2, &klen);

	if (strcmp(k, "Position") == 0)
		luhx_pushvector3(L, glm::vec3(m[3]));

	else if (strcmp(k, "Forward") == 0)
		luhx_pushvector3(L, glm::vec3(m[2]));

	else if (strcmp(k, "Up") == 0)
		luhx_pushvector3(L, glm::vec3(m[1]));

	else if (strcmp(k, "Right") == 0)
		luhx_pushvector3(L, glm::vec3(m[0]));

	else if (klen == 4 && k[0] == 'C' && k[2] == 'R')
	{
		char col = k[1];
		char row = k[3];

		// allows ASCII 49, 50, 51, 52, AKA
		// 1, 2, 3, and 4
		luaL_argcheck(L, col > 48 && col < 53, 2, "column index must be in range [1 .. 4]");
		luaL_argcheck(L, row > 48 && row < 53, 2, "row index must be in range [1 .. 4]");

		lua_pushnumber(L, m[col - 49][row - 49]);
	}
	else
		luaL_error(L, "'%s' is not a valid member of Matrices", k);

	return 1;
}

static int mtx_newindex(lua_State* L)
{
    glm::mat4& m = *(glm::mat4*)luaL_checkudata(L, 1, LUHX_MATRIXLIBNAME);
	size_t klen = 0;
	const char* k = luaL_checklstring(L, 2, &klen);
	float v = static_cast<float>(luaL_checknumber(L, 3));

	if (klen == 4 && k[0] == 'C' && k[2] == 'R')
	{
		char col = k[1];
		char row = k[3];

	    // allows ASCII 49, 50, 51, 52, AKA
	    // 1, 2, 3, and 4
	    luaL_argcheck(L, col > 48 && col < 53, 2, "column index must be in range [1 .. 4]");
	    luaL_argcheck(L, row > 48 && row < 53, 2, "row index must be in range [1 .. 4]");

		m[col - 49][row - 49] = v;
	}
	else
		luaL_error(L, "'%s' is not a valid assignable member of Matrices", k);

	return 0;
}

static int mtx_eq(lua_State* L)
{
    const glm::mat4& a = *(glm::mat4*)luaL_checkudata(L, 1, LUHX_MATRIXLIBNAME);
	const glm::mat4& b = *(glm::mat4*)luaL_checkudata(L, 2, LUHX_MATRIXLIBNAME);

	lua_pushboolean(L, a == b);
	return 1;
}

static int mtx_mul(lua_State* L)
{
    const glm::mat4& a = *(glm::mat4*)luaL_checkudata(L, 1, LUHX_MATRIXLIBNAME);
	const glm::mat4& b = *(glm::mat4*)luaL_checkudata(L, 2, LUHX_MATRIXLIBNAME);

	luhx_pushmatrix(L, a * b);
	return 1;
}

static int mtx_add(lua_State* L)
{
    glm::mat4 a = *(glm::mat4*)luaL_checkudata(L, 1, LUHX_MATRIXLIBNAME);
	const glm::vec3& v = glm::make_vec3(luaL_checkvector(L, 2));

	a[3] += glm::vec4(v, 0.f);

	luhx_pushmatrix(L, a);
	return 1;
}

static int mtx_sub(lua_State* L)
{
    glm::mat4 a = *(glm::mat4*)luaL_checkudata(L, 1, LUHX_MATRIXLIBNAME);
	const glm::vec3& v = glm::make_vec3(luaL_checkvector(L, 2));

	a[3] -= glm::vec4(v, 0.f);

	luhx_pushmatrix(L, a);
	return 1;
}

static void createmetatable(lua_State* L)
{
    luaL_newmetatable(L, LUHX_MATRIXLIBNAME);

	lua_pushstring(L, LUHX_MATRIXLIBNAME);
	lua_setfield(L, -2, "__type");

	lua_pushcfunction(L, mtx_index, "Matrix.__index");
    lua_setfield(L, -2, "__index");

	lua_pushcfunction(L, mtx_newindex, "Matrix.__newindex");
	lua_setfield(L, -2, "__newindex");

	// TODO table key hashes?
	lua_pushcfunction(L, mtx_eq, "Matrix.__eq");
	lua_setfield(L, -2, "__eq");

	lua_pushcfunction(L, mtx_mul, "Matrix.__mul");
	lua_setfield(L, -2, "__mul");

	lua_pushcfunction(L, mtx_add, "Matrix.__add");
	lua_setfield(L, -2, "__add");

	lua_pushcfunction(L, mtx_sub, "Matrix.__sub");
	lua_setfield(L, -2, "__sub");

    lua_pop(L, 1);
}

int luhxopen_Matrix(lua_State* L)
{
    luaL_register(L, LUHX_MATRIXLIBNAME, matrix_funcs);
    createmetatable(L);

    luhx_pushmatrix(L, glm::mat4(1.f));
    lua_setfield(L, -2, "identity");

    return 1;
}
