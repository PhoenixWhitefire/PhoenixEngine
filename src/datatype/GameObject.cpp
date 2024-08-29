#include<algorithm>

#include"datatype/GameObject.hpp"

static uint32_t NumGameObjects = 0;
bool GameObject::s_DidInitReflection = false;
GameObject* GameObject::s_DataModel = nullptr;
std::unordered_map<uint32_t, GameObject*> GameObject::s_WorldArray = {};

GameObject::GameObjectMapType* GameObject::m_gameObjectMap = new GameObjectMapType();

static Reflection::GenericValue destroyObject(Reflection::BaseReflectable* obj, Reflection::GenericValue gv)
{
	dynamic_cast<GameObject*>(obj)->Destroy();
	return Reflection::GenericValue();
}

void GameObject::s_DeclareReflections()
{
	if (s_DidInitReflection)
			//return;
	s_DidInitReflection = true;

	//GameObject::ApiReflection = new Reflection::ReflectionInfo();

	REFLECTION_DECLAREPROP_SIMPLE_READONLY(GameObject, ClassName, String);
	REFLECTION_DECLAREPROP_SIMPLE(GameObject, Name, String);
	REFLECTION_DECLAREPROP_SIMPLE(GameObject, Enabled, Bool);
	REFLECTION_DECLAREPROP_SIMPLE_READONLY(GameObject, ObjectId, Integer);
	
	REFLECTION_DECLAREPROC("Destroy", destroyObject);

	REFLECTION_INHERITAPI(Reflection::Reflectable);
}

GameObject::GameObject()
{
	this->Parent = NULL_GAMEOBJECT_ID;

	GameObject::s_DeclareReflections();
}

GameObject::~GameObject()
{
	if (GameObject* parent = this->GetParent())
		parent->RemoveChild(ObjectId);

	for (GameObject* child : this->GetChildren())
		child->Destroy();

	m_children.clear();

	s_WorldArray.erase(this->ObjectId);

	this->Parent = NULL_GAMEOBJECT_ID;
	//this->ObjectId = NULL_GAMEOBJECT_ID;
	this->ParentLocked = true;
}

bool GameObject::IsValidObjectClass(std::string const& ObjectClass)
{
	GameObjectMapType::iterator it = m_getGameObjectMap()->find(ObjectClass);

	if (it == m_getGameObjectMap()->end())
		return false;
	else
		return true;
}

GameObject* GameObject::GetObjectById(uint32_t Id)
{
	auto it = s_WorldArray.find(Id);
	return it != s_WorldArray.end() ? it->second : nullptr;
}

void GameObject::Initialize()
{
}

void GameObject::Update(double DeltaTime)
{
}

void GameObject::Destroy()
{
	delete this;
}

std::string GameObject::GetFullName()
{
	std::string FullName = this->Name;
	auto curInst = this;

	while (GameObject* parent = curInst->GetParent())
	{
		FullName = parent->Name + "." + FullName;
		curInst = parent;
	}

	return FullName;
}

void GameObject::SetParent(GameObject* newParent)
{
	GameObject* oldParent = GameObject::GetObjectById(Parent);

	if (newParent == oldParent)
		return;

	uint32_t prevId = Parent;

	if (!newParent)
		this->Parent = NULL_GAMEOBJECT_ID;
	else
	{
		this->Parent = newParent->ObjectId;
		newParent->AddChild(this);
	}

	if (oldParent)
		oldParent->RemoveChild(this->ObjectId);
}

void GameObject::AddChild(GameObject* c)
{
	this->m_children.insert(std::pair(c->ObjectId, c->ObjectId));
}

void GameObject::RemoveChild(uint32_t id)
{
	auto it = m_children.find(id);

	if (it != m_children.end())
		m_children.erase(it);
	else
		throw(std::vformat("ID:{} is _not my ({}) sonnn~_", std::make_format_args(ObjectId, id)));
}

GameObject* GameObject::GetParent()
{
	if (this->Parent == NULL_GAMEOBJECT_ID)
		return nullptr;

	auto it = s_WorldArray.find(this->Parent);

	if (it == s_WorldArray.end())
	{
		this->Parent = NULL_GAMEOBJECT_ID;
		return nullptr;
	}
	else
		return it->second;
}

std::vector<GameObject*> GameObject::GetChildren()
{
	// 27/08/2024: A destroyed Object keeps getting destructed twice somewhere
	if (this->ObjectId == NULL_GAMEOBJECT_ID)
		return {};

	std::vector<GameObject*> children;

	for (auto& childEntry : this->m_children)
	{
		uint32_t childId = childEntry.second;

		GameObject* child = GameObject::GetObjectById(childId);

		if (child)
			children.push_back(child);
		else
			m_children.erase(childEntry.first);
	}

	return children;
}

GameObject* GameObject::GetChildById(uint32_t id)
{
	auto it = m_children.find(id);

	return it != m_children.end() ? s_WorldArray[id] : nullptr;
}

GameObject* GameObject::GetChild(std::string const& ChildName)
{
	for (auto& it : m_children)
	{
		GameObject* child = GameObject::GetObjectById(it.second);

		if (!child)
		{
			m_children.erase(it.first);
			continue;
		}

		if (child->Name == ChildName)
			return child;
	}

	return nullptr;
}


GameObject* GameObject::GetChildOfClass(std::string const& Class)
{
	for (auto& it : m_children)
	{
		GameObject* child = this->GetChildById(m_children[it.second]);

		if (!child)
		{
			this->m_children.erase(it.first);
			continue;
		}

		if (child->ClassName == Class)
			return child;
	}

	return nullptr;
}

GameObject* GameObject::CreateGameObject(std::string const& ObjectClass)
{
	GameObjectMapType::iterator it = m_getGameObjectMap()->find(ObjectClass);

	if (it == m_getGameObjectMap()->end())
		throw(std::vformat(
			"Attempted to create invalid GameObject '{}'!",
			std::make_format_args(ObjectClass)
		));

	GameObject* CreatedObject = it->second();
	// ID:0 is a reserved ID
	// Whenever anyone tries to use it, we know that an uninitialized
	// Object was involved.
	// To indicate a NULL Object, use `NULL_GAMEOBJECT_ID`.
	// Thus, add 1 so that the first create Object has ID 1 and not 0.
	CreatedObject->ObjectId = NumGameObjects + 1;
	NumGameObjects++;

	s_WorldArray.insert(std::pair(CreatedObject->ObjectId, CreatedObject));

	CreatedObject->Initialize();

	return CreatedObject;
}

GameObject::GameObjectMapType* GameObject::m_getGameObjectMap()
{
	// Copied from StackOverflow post:
	// 
	// never delete'ed. (exist until program termination)
	// because we can't guarantee correct destruction order 
	if (!m_gameObjectMap)
		m_gameObjectMap = new GameObjectMapType;

	return m_gameObjectMap;
}

nlohmann::json GameObject::DumpApiToJson()
{
	nlohmann::json dump{};
	
	for (auto& g : *GameObject::m_getGameObjectMap())
	{
		auto newobj = g.second();
		dump[g.first] = nlohmann::json();

		for (auto& p : newobj->ApiReflection->GetProperties())
			dump[g.first][p.first] = Reflection::TypeAsString(p.second.Type);

		for (auto& f : newobj->ApiReflection->GetFunctions())
		{
			std::string istring = "";
			std::string ostring = "";

			for (Reflection::ValueType i : f.second.Inputs)
				istring += Reflection::TypeAsString(i) + ",";

			for (Reflection::ValueType o : f.second.Outputs)
				ostring += Reflection::TypeAsString(o) + ",";

			dump[g.first][f.first] = std::vformat("({}) -> ({})", std::make_format_args(istring, ostring));
		}

		delete newobj;
	}

	return dump;
}
