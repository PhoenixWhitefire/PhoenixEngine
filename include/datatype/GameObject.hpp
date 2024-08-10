#pragma once

#define PHX_ASSERT(res, err) if (!res) throw(err)

#include<functional>
#include<vector>
#include<string>
#include<map>

#include"Reflection.hpp"
#include"datatype/Vector3.hpp"
#include"datatype/Color.hpp"
#include"datatype/Event.hpp"

class GameObject : public Reflection::Reflectable
{
public:
	GameObject();
	~GameObject();

	std::vector<std::shared_ptr<GameObject>>& GetChildren();
	std::shared_ptr<GameObject> GetChildOfClass(std::string lass);

	void SetParent(std::shared_ptr<GameObject> Parent);
	void AddChild(std::shared_ptr<GameObject> Child);
	void RemoveChild(uint32_t ObjectId);

	virtual void Initialize();
	virtual void Update(double DeltaTime);

	std::shared_ptr<GameObject> GetChild(std::string ChildName);

	std::string GetFullName();

	static std::shared_ptr<GameObject> DataModel;

	std::string Name = "GameObject";
	std::string ClassName = "GameObject";

	bool DidInit = false;

	std::shared_ptr<GameObject> operator [] (std::string ChildName)
	{
		for (int Index = 0; Index < this->m_children.size(); Index++)
			if (this->m_children[Index]->Name == ChildName)
				return this->m_children[Index];

		return nullptr;
	}

	bool operator ! ()
	{
		return false;
	}

	std::shared_ptr<GameObject> Parent;

	bool Enabled = true;

	Event<std::shared_ptr<GameObject>> OnChildAdded;
	Event<std::shared_ptr<GameObject>> OnChildRemoving;

	bool ParentLocked = false;

	int32_t ObjectId = 0;

protected:
	std::vector<std::shared_ptr<GameObject>> m_children;
};

// I followed this StackOverflow post:
// https://stackoverflow.com/a/582456/16875161

struct GameObjectFactory
{
	static std::shared_ptr<GameObject> CreateGameObject(std::string const& ObjectClass);

	// TODO: understand contructor type voodoo, + is this even a good method?
	typedef std::map<std::string, std::shared_ptr<GameObject>(*)()> GameObjectMapType;

	static GameObjectMapType* GetGameObjectMap();

	static GameObjectMapType* GameObjectMap;
};

template<typename T> std::shared_ptr<GameObject> createT_baseGameObject()
{
	return std::shared_ptr<GameObject>(new T);
}

template <typename T>
struct DerivedObjectRegister : GameObjectFactory
{
	DerivedObjectRegister(std::string const& ObjectClass)
	{
		GetGameObjectMap()->insert(std::make_pair(ObjectClass, &createT_baseGameObject<T>));
	}
};
