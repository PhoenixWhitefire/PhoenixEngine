// luhx.hpp
// Phoenix Engine specific Luau libraries

#include <lua.h>
#include <lualib.h>

void luhx_openlibs(lua_State*);

int luhxopen_base(lua_State*);

#define LUHX_FSLIBNAME "fs"
int luhxopen_fs(lua_State*);

#define LUHX_NETLIBNAME "net"
int luhxopen_net(lua_State*);

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

#define LUHX_WORLDLIBNAME "world"
int luhxopen_world(lua_State*);
