#pragma once

#include<memory>
#include<vector>
#include<string>
#include<map>

#include<datatype/Event.hpp>

class GameObject
{
public:
	GameObject();

	//static GameObject* New(std::string Type);
	//static GameObject* New(const char* Type);

	std::vector<std::shared_ptr<GameObject>> GetChildren();
	std::shared_ptr<GameObject> GetChildOfClass(std::string lass);

	std::string Name = "BaseGameObject";
	std::string ClassName = "GameObject";

	std::shared_ptr<GameObject> Parent;

	bool Enabled = true;

	void SetParent(std::shared_ptr<GameObject> Parent);
	void AddChild(std::shared_ptr<GameObject> Child);
	void RemoveChild(unsigned int ObjectId);

	Event<std::shared_ptr<GameObject>> OnChildAdded;
	Event<std::shared_ptr<GameObject>> OnChildRemoving;

	virtual void Initialize();
	virtual void Update(double DeltaTime);

	std::shared_ptr<GameObject> GetChild(std::string ChildName);

	void* GetProperty(std::string Property);
	std::vector<std::pair<std::string, void*>> GetProperties();

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

	bool ParentLocked = false;

	unsigned int GameObjectId = 0;

	virtual ~GameObject() = default;

protected:
	std::vector<std::shared_ptr<GameObject>> m_children;

	std::unordered_map<std::string, void*> m_properties;
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
