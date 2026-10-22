// luhx.hpp
// Phoenix Engine specific Luau libraries

#pragma once

#include <glm/mat4x4.hpp>
#include <cstdint>
#include "lua.h"

#include "datatype/EntityComponent.hpp"
#include "datatype/Ref.hpp"

struct InputEvent;
class Color;

namespace Reflection
{
	struct EventDescriptor;
};

void luhx_openlibs(lua_State*);

int luhxopen_base(lua_State*);

int luhxopen_ecs(lua_State*);
int luhxopen_world(lua_State*);

int luhxopen_fs(lua_State*);
int luhxopen_imgui(lua_State*);
int luhxopen_json(lua_State*);
int luhxopen_task(lua_State*);
int luhxopen_parallel(lua_State*);
int luhxopen_system(lua_State*);
int luhxopen_schedule(lua_State*);
int luhxopen_profiler(lua_State*);

#define LUHX_ENUMLIBNAME "Enum"
int luhxopen_Enum(lua_State*);

#define LUHX_COLORLIBNAME "Color"
int luhxopen_Color(lua_State*);

#define LUHX_MATRIXLIBNAME "Matrix"
int luhxopen_Matrix(lua_State*);

static const inline std::unordered_map<std::string_view, lua_CFunction> BuiltinLibraries = {
	{ "ecs", luhxopen_ecs },
	{ "world", luhxopen_world },
	{ "fs", luhxopen_fs },
	{ "imgui", luhxopen_imgui },
	{ "json", luhxopen_json },
	{ "task", luhxopen_task },
	{ "parallel", luhxopen_parallel },
	{ "system", luhxopen_system },
	{ "schedule", luhxopen_schedule },
	{ "profiler", luhxopen_profiler },
};

#define LUHX_GAMEOBJECTLIBNAME "GameObject"
int luhxopen_GameObject(lua_State*);

int luhxopen_EventSignal(lua_State*);
int luhxopen_EventConnection(lua_State*);
int luhxopen_InputEvent(lua_State*);

void luhx_pushgameobject(lua_State*, GameObject*);
void luhx_pushvector3(lua_State*, const glm::vec3&);
void luhx_pushmatrix(lua_State*, const glm::mat4&);
void luhx_pushcolor(lua_State*, const Color&);
void luhx_pushsignal(lua_State*, const Reflection::EventDescriptor*, const ReflectorRef&, const char*, uint32_t RestrictDataModel);
void luhx_pushinputevent(lua_State*, const InputEvent&);

struct World;
World* luhx_checkworld(lua_State* L, int idx);

using Id = uint64_t;
Id luhx_checkid(lua_State* L, int idx);
void luhx_pushid(lua_State* L, Id id);

struct ComponentRecordBase;

struct ComponentUserdata
{
	ComponentRecordBase* cr = nullptr;
	uint64_t index = UINT64_MAX;
};

const ComponentUserdata* luhx_checkcomponent(lua_State* L, int idx);
void luhx_pushcomponent(lua_State* L, ComponentRecordBase* cr, uint64_t index);

int luhx_makemodule(lua_State* L, const struct luaL_Reg* l);

GameObject* luhx_checkgameobject(lua_State*, int);

struct EventSignalData
{
	ReflectorRef Reflector;
	const Reflection::EventDescriptor* Event = nullptr;
	const char* EventName = nullptr;
	uint32_t RestrictDataModel = UINT32_MAX;
};

struct EventConnectionData
{
	ReflectorRef Reflector;
	ObjectRef DataModel;
	const Reflection::EventDescriptor* Event = nullptr;
	lua_State* L = nullptr;
	uint32_t ConnectionId = UINT32_MAX;
	int SpawningThreadRef = -1;
	int EThreadRef = -1;
	int CThreadRef = -1;
	int SignalRef = -1;
};
