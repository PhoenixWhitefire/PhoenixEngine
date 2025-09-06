#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "datatype/GameObject.hpp"
#include "component/Transform.hpp"
#include "component/Mesh.hpp"
#include "IntersectionLib.hpp"

static int world_raycast(lua_State* L)
{
    GameObject* workspace = GameObject::GetObjectById(GameObject::s_DataModel)->FindChildWhichIsA("Workspace");

	if (!workspace)
		luaL_error(L, "A Workspace was not found within the DataModel");

	glm::vec3 origin = ScriptEngine::L::ToGeneric(L, 1).AsVector3();
	glm::vec3 vector = ScriptEngine::L::ToGeneric(L, 2).AsVector3();

	Reflection::GenericValue ignoreListVal = ScriptEngine::L::ToGeneric(L, 3);
	std::span<Reflection::GenericValue> providedIgnoreList = ignoreListVal.AsArray();

	std::vector<GameObject*> ignoreList;
	for (const Reflection::GenericValue& gv : providedIgnoreList)
		ignoreList.push_back(GameObject::FromGenericValue(gv));

	IntersectionLib::Intersection result;
	GameObject* hitObject = nullptr;
	double closestHit = INFINITY;

	for (GameObject* p : workspace->GetDescendants())
	{
		if (std::find(ignoreList.begin(), ignoreList.end(), p) != ignoreList.end())
			continue;
    
		EcMesh* object = p->GetComponent<EcMesh>();
		EcTransform* ct = p->GetComponent<EcTransform>();
    
		if (object && ct)
		{
			glm::vec3 pos = ct->Transform[3];
			glm::vec3 size = ct->Size;
        
			IntersectionLib::Intersection hit = IntersectionLib::LineAabb(
				origin,
				vector,
				pos,
				size
			);
        
			if (hit.Occurred)
				if (hit.Depth < closestHit)
				{
					result = hit;
					closestHit = hit.Depth;
					hitObject = p;
				}
		}
	}

	if (hitObject)
	{
		lua_newtable(L);
    
		ScriptEngine::L::PushGameObject(L, hitObject);
		lua_setfield(L, -2, "Object");
    
		Reflection::GenericValue posg{ result.Position };
    
		ScriptEngine::L::PushGenericValue(L, posg);
		lua_setfield(L, -2, "Position");
    
		Reflection::GenericValue normalg{ result.Normal };
    
		ScriptEngine::L::PushGenericValue(L, normalg);
		lua_setfield(L, -2, "Normal");
	}
	else
		lua_pushnil(L);

	return 1;
}

static int world_aabbcast(lua_State* L)
{
    GameObject* workspace = GameObject::GetObjectById(GameObject::s_DataModel)->FindChildWhichIsA("Workspace");

	if (!workspace)
		luaL_error(L, "A Workspace was not found within the DataModel");

	glm::vec3 apos = ScriptEngine::L::ToGeneric(L, 1).AsVector3();
	glm::vec3 asize = ScriptEngine::L::ToGeneric(L, 2).AsVector3();

	Reflection::GenericValue ignoreListVal = ScriptEngine::L::ToGeneric(L, 3);
	std::span<Reflection::GenericValue> providedIgnoreList = ignoreListVal.AsArray();

	std::vector<GameObject*> ignoreList;
	for (const Reflection::GenericValue& gv : providedIgnoreList)
		ignoreList.push_back(GameObject::FromGenericValue(gv));

	IntersectionLib::Intersection result;
	GameObject* hitObject = nullptr;
	double closestHit = INFINITY;

	for (GameObject* p : workspace->GetDescendants())
	{
		if (std::find(ignoreList.begin(), ignoreList.end(), p) != ignoreList.end())
			continue;
    
		EcMesh* object = p->GetComponent<EcMesh>();
		EcTransform* ct = p->GetComponent<EcTransform>();
    
		if (object && ct)
		{
			glm::vec3 bpos = ct->Transform[3];
			glm::vec3 bsize = ct->Size;
        
			IntersectionLib::Intersection hit = IntersectionLib::AabbAabb(
				apos,
				asize,
				bpos,
				bsize
			);
        
			if (hit.Occurred)
				if (hit.Depth < closestHit)
				{
					result = hit;
					closestHit = hit.Depth;
					hitObject = p;
				}
		}
	}

	if (hitObject)
	{
		lua_newtable(L);
    
		ScriptEngine::L::PushGameObject(L, hitObject);
		lua_setfield(L, -2, "Object");
    
		Reflection::GenericValue posg = result.Position;
    
		ScriptEngine::L::PushGenericValue(L, posg);
		lua_setfield(L, -2, "Position");
    
		Reflection::GenericValue normalg = result.Normal;
    
		ScriptEngine::L::PushGenericValue(L, normalg);
		lua_setfield(L, -2, "Normal");
	}
	else
		lua_pushnil(L);

	return 1;
}

static int world_aabbquery(lua_State* L)
{
    GameObject* workspace = GameObject::GetObjectById(GameObject::s_DataModel)->FindChildWhichIsA("Workspace");

	if (!workspace)
		luaL_error(L, "A Workspace was not found within the DataModel");

	glm::vec3 apos = ScriptEngine::L::ToGeneric(L, 1).AsVector3();
	glm::vec3 asize = ScriptEngine::L::ToGeneric(L, 2).AsVector3();

	Reflection::GenericValue ignoreListVal = ScriptEngine::L::ToGeneric(L, 3);
	std::span<Reflection::GenericValue> providedIgnoreList = ignoreListVal.AsArray();

	std::vector<GameObject*> ignoreList;
	for (const Reflection::GenericValue& gv : providedIgnoreList)
		ignoreList.push_back(GameObject::FromGenericValue(gv));

	IntersectionLib::Intersection result;
	std::vector<GameObject*> hits;

	for (GameObject* p : workspace->GetDescendants())
	{
		if (std::find(ignoreList.begin(), ignoreList.end(), p) != ignoreList.end())
			continue;
    
		EcTransform* object = p->GetComponent<EcTransform>();
    
		if (object)
		{
			glm::vec3 bpos = object->Transform[3];
			glm::vec3 bsize = object->Size;
        
			IntersectionLib::Intersection hit = IntersectionLib::AabbAabb(
				apos,
				asize,
				bpos,
				bsize
			);
        
			if (hit.Occurred)
				hits.push_back(p);
		}
	}

	lua_newtable(L);

	for (int index = 0; static_cast<size_t>(index) < hits.size(); index++)
	{
		lua_pushinteger(L, index);
		ScriptEngine::L::PushGameObject(L, hits[index]);
		lua_settable(L, -3);
	}

	return 1;
}

static luaL_Reg world_funcs[] =
{
    { "raycast", world_raycast },
    { "aabbcast", world_aabbcast },
    { "aabbquery", world_aabbquery },
    { NULL, NULL }
};

int luhxopen_world(lua_State* L)
{
    luaL_register(L, LUHX_WORLDLIBNAME, world_funcs);

    return 1;
}
