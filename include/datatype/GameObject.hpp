#pragma once

#include<functional>
#include<vector>
#include<string>
#include<map>

#include"datatype/Vector3.hpp"
#include"datatype/Color.hpp"
#include"datatype/Event.hpp"

// pls trust me im not this revolting irl 13/06/2024
#define PHX_GetPropValue(GenericType)    \
{                                        \
	switch (GenericType.Type) :          \
		case (0):                        \
			return NULL;                 \
		case (1):                        \
			return GenericType.Double;   \
		case (2):                        \
			return GenericType.Float;    \
		case(3):                         \
			return GenericType.Integer;  \
		case(4):                         \
			return GenericType.String;   \
}

enum class PropType {
	NONE,
	String,
	Bool,
	Double,
	Integer,
	Color,
	Vector3
};

// i just cant 13/06/2024
struct GenericType
{
	PropType Type;
	std::string String;
	bool Bool;
	double Double;
	int Integer;
	Color Color3;
	Vector3 Vector3;
};

typedef std::pair<std::function<GenericType(void)>, std::function<void(GenericType)>> PropGetSet_t;
typedef std::pair<PropType, PropGetSet_t> PropDef_t;
typedef std::unordered_map<std::string, PropDef_t> PropList_t;
typedef std::pair<std::string, PropDef_t> PropListItem_t;

class GameObject
{
public:
	GameObject();
	~GameObject();

	//static GameObject* New(std::string Type);
	//static GameObject* New(const char* Type);

	std::vector<std::shared_ptr<GameObject>> GetChildren();
	std::shared_ptr<GameObject> GetChildOfClass(std::string lass);

	void SetParent(std::shared_ptr<GameObject> Parent);
	void AddChild(std::shared_ptr<GameObject> Child);
	void RemoveChild(uint32_t ObjectId);

	virtual void Initialize();
	virtual void Update(double DeltaTime);

	std::shared_ptr<GameObject> GetChild(std::string ChildName);

	PropList_t GetProperties();

	GenericType GetName();
	GenericType GetClassName();
	GenericType GetEnabled();

	void SetName(std::string);
	void SetEnabled(bool);

	std::string Name = "GameObject";
	std::string ClassName = "GameObject";

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

	uint32_t GameObjectId = 0;

protected:
	std::vector<std::shared_ptr<GameObject>> m_children;

	PropList_t m_properties;
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
