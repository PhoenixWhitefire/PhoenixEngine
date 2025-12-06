// luhx.hpp
// Phoenix Engine specific Luau libraries

#include <lua.h>
#include <lualib.h>
#include <glm/mat4x4.hpp>

#include "datatype/GameObject.hpp"
#include "datatype/Color.hpp"

void luhx_openlibs(lua_State*);

int luhxopen_base(lua_State*);

#define LUHX_FSLIBNAME "fs"
int luhxopen_fs(lua_State*);

#define LUHX_CONFLIBNAME "conf"
int luhxopen_conf(lua_State*);

#define LUHX_IMGUILIBNAME "imgui"
int luhxopen_imgui(lua_State*);

#define LUHX_INPUTLIBNAME "input"
int luhxopen_input(lua_State*);

#define LUHX_JSONLIBNAME "json"
int luhxopen_json(lua_State*);

#define LUHX_MESHLIBNAME "mesh"
int luhxopen_mesh(lua_State*);

#define LUHX_MODELLIBNAME "model"
int luhxopen_model(lua_State*);

#define LUHX_SCENELIBNAME "scene"
int luhxopen_scene(lua_State*);

#define LUHX_COLORLIBNAME "Color"
int luhxopen_Color(lua_State*);

#define LUHX_MATRIXLIBNAME "Matrix"
int luhxopen_Matrix(lua_State*);

#define LUHX_GAMEOBJECTLIBNAME "GameObject"
int luhxopen_GameObject(lua_State*);

int luhxopen_EventSignal(lua_State*);
int luhxopen_EventConnection(lua_State*);

void luhx_pushgameobject(lua_State*, const GameObject*);
void luhx_pushvector3(lua_State*, const glm::vec3&);
void luhx_pushmatrix(lua_State*, const glm::mat4&);
void luhx_pushcolor(lua_State*, const Color&);
void luhx_pushsignal(lua_State*, const Reflection::EventDescriptor*, const ReflectorRef&, const char*);

GameObject* luhx_checkgameobject(lua_State*, int);

struct EventSignalData
{
	ReflectorRef Reflector;
	const Reflection::EventDescriptor* Event = nullptr;
	const char* EventName = nullptr;
};

struct EventConnectionData
{
	ReflectorRef Reflector;
	ObjectHandle Script;
	const Reflection::EventDescriptor* Event = nullptr;
	lua_State* L = nullptr;
	uint32_t ConnectionId = UINT32_MAX;
	int ThreadRef = LUA_NOREF;
	int SignalRef = LUA_NOREF;
	bool CallbackYields = false;
};
