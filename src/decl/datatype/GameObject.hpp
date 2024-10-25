#pragma once

#include <stdint.h>

#define PHX_ASSERT(res, err) if (!res) throw(err)
#define PHX_GAMEOBJECT_NULL_ID UINT32_MAX

// link a class name to a C++ GameObject-deriving class
#define PHX_GAMEOBJECT_LINKTOCLASS(strclassname, cclass) static RegisterDerivedObject<cclass> Register##cclass(strclassname);
// shorthand for `PHX_GAMEOBJECT_LINKTOCLASS`. the resulting class name is the same as passed in,
// and the C++ class is `Object_<Class>`
#define PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(classname) PHX_GAMEOBJECT_LINKTOCLASS(#classname, Object_##classname)

// 01/09/2024:
// MUST be added to the `public` section of *all* objects so
// any APIs they declare can be found
#define PHX_GAMEOBJECT_API_REFLECTION static const PropertyMap& s_GetProperties()  \
{ \
	return s_Api.Properties; \
} \
 \
static const FunctionMap& s_GetFunctions() \
{ \
	return s_Api.Functions; \
} \
static const std::vector<std::string>& s_GetLineage() \
{ \
	return s_Api.Lineage; \
} \
virtual const PropertyMap& GetProperties()  \
{ \
	return s_Api.Properties; \
} \
 \
virtual const FunctionMap& GetFunctions() \
{ \
	return s_Api.Functions; \
} \
virtual const std::vector<std::string>& GetLineage() \
{ \
	return s_Api.Lineage; \
} \
virtual bool HasProperty(const std::string & MemberName) \
{ \
	return s_Api.Properties.find(MemberName) != s_Api.Properties.end(); \
} \
 \
virtual bool HasFunction(const std::string & MemberName) \
{ \
	return s_Api.Functions.find(MemberName) != s_Api.Functions.end(); \
} \
 \
virtual const IProperty& GetProperty(const std::string & MemberName) \
{ \
	return HasProperty(MemberName) \
		? s_Api.Properties[MemberName] \
		: throw(std::string("Invalid Property in GetProperty ") + MemberName); \
} \
 \
virtual const IFunction& GetFunction(const std::string & MemberName) \
{ \
	return HasFunction(MemberName) \
		? s_Api.Functions[MemberName] \
		: throw(std::string("Invalid Function in GetFunction ") + MemberName); \
} \
 \
virtual Reflection::GenericValue GetPropertyValue(const std::string & MemberName) \
{ \
	return HasProperty(MemberName) \
		? s_Api.Properties[MemberName].Get(this) \
		: throw(std::string("Invalid Property in GetPropertyValue ") + MemberName); \
} \
 \
virtual void SetPropertyValue(const std::string & MemberName, const Reflection::GenericValue & NewValue) \
{ \
	if (HasProperty(MemberName)) \
		s_Api.Properties[MemberName].Set(this, NewValue); \
	else \
		throw(std::string("Invalid Property in SetPropertyValue ") + MemberName); \
} \
 \
virtual Reflection::GenericValue CallFunction(const std::string& MemberName, const std::vector<Reflection::GenericValue>& Param) \
{ \
	if (HasFunction(MemberName)) \
		return s_Api.Functions[MemberName].Func(this, Param); \
	else \
		throw(std::string("InvalidFunction in CallFunction " + MemberName)); \
} \

#include<functional>
#include<vector>
#include<string>
#include<unordered_map>
#include<nljson.hpp>

#include"Reflection.hpp"
#include"datatype/Event.hpp"

class GameObject;

struct IProperty
{
	Reflection::ValueType Type{};

	std::function<Reflection::GenericValue(GameObject*)> Get;
	std::function<void(GameObject*, const Reflection::GenericValue&)> Set;
};

struct IFunction
{
	std::vector<Reflection::ValueType> Inputs;
	std::vector<Reflection::ValueType> Outputs;

	std::function<std::vector<Reflection::GenericValue>(GameObject*, const std::vector<Reflection::GenericValue>&)> Func;
};

typedef std::unordered_map<std::string, IProperty> PropertyMap;
typedef std::unordered_map<std::string, IFunction> FunctionMap;

struct Api
{
	PropertyMap Properties;
	FunctionMap Functions;
	std::vector<std::string> Lineage;
};

class GameObject
{
public:
	GameObject();
	virtual ~GameObject();

	GameObject(GameObject&) = delete;

	PHX_GAMEOBJECT_API_REFLECTION;

	static GameObject* FromGenericValue(const Reflection::GenericValue&);

	static bool IsValidObjectClass(std::string const&);
	static GameObject* Create(std::string const&);
	static GameObject* GetObjectById(uint32_t);

	static GameObject* s_DataModel;
	static std::unordered_map<uint32_t, GameObject*> s_WorldArray;

	virtual void Initialize();
	virtual void Update(double DeltaTime);

	std::vector<GameObject*> GetChildren();
	std::vector<GameObject*> GetDescendants();

	GameObject* GetParent();
	GameObject* GetChild(std::string const&);
	// accounts for inheritance
	GameObject* GetChildOfClass(std::string const&);
	GameObject* GetChildById(uint32_t);

	std::string GetFullName();
	// whether this object inherits from or is the given class
	bool IsA(const std::string&);

	void Destroy();

	void SetParent(GameObject*);
	void AddChild(GameObject*);
	void RemoveChild(uint32_t);

	Reflection::GenericValue ToGenericValue();

	uint32_t ObjectId = 0;

	std::string Name = "GameObject";
	std::string ClassName = "GameObject";

	bool Enabled = true;
	bool ParentLocked = false;

	uint32_t Parent = PHX_GAMEOBJECT_NULL_ID;

	static nlohmann::json DumpApiToJson();

protected:
	std::vector<uint32_t> m_Children;

	// I followed this StackOverflow post:
	// https://stackoverflow.com/a/582456/16875161

	// Needs to be in `protected` because `RegisterDerivedObject`
	typedef std::unordered_map<std::string, GameObject* (*)()> GameObjectMapType;
	static inline GameObjectMapType* s_GameObjectMap = new GameObjectMapType;

private:
	static void s_DeclareReflections();
	static inline Api s_Api{};
};

template<typename T> GameObject* constructGameObjectHeir()
{
	return new T;
}

template <typename T>
struct RegisterDerivedObject : GameObject
{
	RegisterDerivedObject(std::string const& ObjectClass)
	{
		s_GameObjectMap->insert(std::make_pair(ObjectClass, &constructGameObjectHeir<T>));
	}
};

typedef GameObject Object_GameObject;
