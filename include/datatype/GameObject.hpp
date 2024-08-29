#pragma once

#define PHX_ASSERT(res, err) if (!res) throw(err)
#define NULL_GAMEOBJECT_ID 0xFFFFFFu

#include<functional>
#include<vector>
#include<string>
#include<unordered_map>
#include<nljson.hpp>

#include"Reflection.hpp"
#include"datatype/Vector3.hpp"
#include"datatype/Color.hpp"
#include"datatype/Event.hpp"

class GameObject : public Reflection::Reflectable
{
public:
	GameObject();
	virtual ~GameObject();

	static bool IsValidObjectClass(std::string const&);
	static GameObject* CreateGameObject(std::string const&);
	static GameObject* GetObjectById(uint32_t);

	static GameObject* s_DataModel;
	static std::unordered_map<uint32_t, GameObject*> s_WorldArray;

	virtual void Initialize();
	virtual void Update(double DeltaTime);

	std::vector<GameObject*> GetChildren();

	GameObject* GetParent();
	GameObject* GetChild(std::string const&);
	// EXACT match, does not account for inheritance
	GameObject* GetChildOfClass(std::string const&);
	GameObject* GetChildById(uint32_t);

	std::string GetFullName();

	void Destroy();

	void SetParent(GameObject*);
	void AddChild(GameObject*);
	void RemoveChild(uint32_t);

	uint32_t ObjectId = 0;

	std::string Name = "GameObject";
	std::string ClassName = "GameObject";

	bool Enabled = true;
	bool ParentLocked = false;

	uint32_t Parent;

	EventSignal<GameObject*> OnChildAdded;
	EventSignal<GameObject*> OnChildRemoving;

	static nlohmann::json DumpApiToJson();

protected:
	std::unordered_map<uint32_t, uint32_t> m_children;

	// I followed this StackOverflow post:
	// https://stackoverflow.com/a/582456/16875161

	// Needs to be in `protected` because `RegisterDerivedObject`
	typedef std::unordered_map<std::string, GameObject* (*)()> GameObjectMapType;
	static GameObjectMapType* m_getGameObjectMap();
	static GameObjectMapType* m_gameObjectMap;

private:
	static void s_DeclareReflections();
	static bool s_DidInitReflection;
};

template<typename T> GameObject* createT_baseGameObject()
{
	return dynamic_cast<GameObject*>(new T);
}

template <typename T>
struct RegisterDerivedObject : GameObject
{
	RegisterDerivedObject(std::string const& ObjectClass)
	{
		m_getGameObjectMap()->insert(std::make_pair(ObjectClass, &createT_baseGameObject<T>));
	}
};
