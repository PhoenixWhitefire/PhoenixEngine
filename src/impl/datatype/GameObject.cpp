#include<algorithm>

#include"datatype/GameObject.hpp"
#include"Debug.hpp"

static uint32_t NumGameObjects = 0;
static bool s_DidInitReflection = false;
GameObject* GameObject::s_DataModel = nullptr;
std::unordered_map<uint32_t, GameObject*> GameObject::s_WorldArray = {};

static RegisterDerivedObject<GameObject> RegisterClassAs("GameObject");

static void destroyObject(GameObject* obj)
{
	dynamic_cast<GameObject*>(obj)->Destroy();
}

static std::string getFullName(GameObject* object)
{
	std::string fullName = object->Name;
	auto curObject = object;

	while (GameObject* parent = curObject->GetParent())
	{
		if (parent == GameObject::s_DataModel)
			break;
		fullName = parent->Name + "." + fullName;
		curObject = parent;
	}

	return fullName;
}

void GameObject::s_DeclareReflections()
{
	if (s_DidInitReflection)
			return;
	s_DidInitReflection = true;

	REFLECTION_DECLAREPROP_SIMPLE_READONLY(GameObject, ClassName, String);
	REFLECTION_DECLAREPROP_SIMPLE(GameObject, Name, String);
	REFLECTION_DECLAREPROP_SIMPLE(GameObject, Enabled, Bool);
	REFLECTION_DECLAREPROP_SIMPLE_READONLY(GameObject, ObjectId, Integer);
	REFLECTION_DECLAREPROP(
		"Parent",
		GameObject,
		[](GameObject* p)
		{
			// This is OK even if `->GetParent()` returns `nullptr`,
			// because `::ToGenericValue` accounts for when `this` is `nullptr`
			// 06/10/2024
			return p->GetParent()->ToGenericValue();
			/*
			Reflection::GenericValue gv = p->GetParent() ? p->GetParent()->ObjectId : PHX_GAMEOBJECT_NULL_ID;
			gv.Type = Reflection::ValueType::GameObject;
			return gv;
			*/
		},
		[](GameObject* p, const Reflection::GenericValue& gv)
		{
			if (p->Parent == gv.AsInteger())
				return;

			GameObject* newParent = GameObject::GetObjectById(static_cast<uint32_t>(gv.AsInteger()));

			if (!newParent)
				p->SetParent(nullptr);
			else
			{
				if (newParent != p)
				{
					std::vector<GameObject*> descendants = p->GetDescendants();

					bool isOwnDescendant = false;

					for (GameObject* d : descendants)
						if (d == newParent)
						{
							isOwnDescendant = true;
							break;
						}

					if (!isOwnDescendant)
						p->SetParent(newParent);
					else
						Debug::Log(std::vformat(
							"Tried to make object ID:{} a descendant of itself",
							std::make_format_args(p->ObjectId)
						));
				}
				else
					Debug::Log(std::vformat(
						"Tried to make object ID:{} it's own parent",
						std::make_format_args(p->ObjectId)
					));
			}
		}
	);
	
	REFLECTION_DECLAREPROC_INPUTLESS(Destroy, destroyObject);
	REFLECTION_DECLAREFUNC(
		"GetFullName",
		{},
		{ Reflection::ValueType::String },
		[](GameObject* object, const std::vector<Reflection::GenericValue>&)
		-> std::vector<Reflection::GenericValue>
		{
			return { getFullName(object) };
		}
	);

	//REFLECTION_INHERITAPI(Reflection::Reflectable);
}

GameObject::GameObject()
{
	this->Parent = PHX_GAMEOBJECT_NULL_ID;

	GameObject::s_DeclareReflections();
}

GameObject* GameObject::FromGenericValue(const Reflection::GenericValue& gv)
{
	if (gv.Type == Reflection::ValueType::Null)
		return nullptr;

	if (gv.Type != Reflection::ValueType::GameObject)
	{
		const std::string& typeName = Reflection::TypeAsString(gv.Type);

		throw(std::vformat(
			"Tried to GameObject::FromGenericValue, but GenericValue had Type '{}' instead",
			std::make_format_args(typeName)
		));
	}

	return GameObject::GetObjectById(static_cast<uint32_t>((int64_t)gv.Value));
}

GameObject::~GameObject()
{
	if (GameObject* parent = this->GetParent())
		parent->RemoveChild(ObjectId);

	for (GameObject* child : this->GetChildren())
		child->Destroy();

	m_Children.clear();

	s_WorldArray.erase(this->ObjectId);

	this->Parent = PHX_GAMEOBJECT_NULL_ID;
	//this->ObjectId = PHX_GAMEOBJECT_NULL_ID;
	this->ParentLocked = true;
}

bool GameObject::IsValidObjectClass(std::string const& ObjectClass)
{
	GameObjectMapType::iterator it = s_GameObjectMap->find(ObjectClass);

	if (it == s_GameObjectMap->end())
		return false;
	else
		return true;
}

GameObject* GameObject::GetObjectById(uint32_t Id)
{
	if (Id == PHX_GAMEOBJECT_NULL_ID)
		return nullptr;

	auto it = s_WorldArray.find(Id);
	return it != s_WorldArray.end() ? it->second : nullptr;
}

void GameObject::Initialize()
{
}

void GameObject::Update(double)
{
}

void GameObject::Destroy()
{
	delete this;
}

std::string GameObject::GetFullName()
{
	return getFullName(this);
}

void GameObject::SetParent(GameObject* newParent)
{
	GameObject* oldParent = GameObject::GetObjectById(Parent);

	if (newParent == oldParent)
		return;

	if (!newParent)
		this->Parent = PHX_GAMEOBJECT_NULL_ID;
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
	m_Children.insert(std::pair(c->ObjectId, c->ObjectId));
}

void GameObject::RemoveChild(uint32_t id)
{
	auto it = m_Children.find(id);

	if (it != m_Children.end())
		m_Children.erase(it);
	else
		throw(std::vformat("ID:{} is _not my ({}) sonnn~_", std::make_format_args(ObjectId, id)));
}

Reflection::GenericValue GameObject::ToGenericValue()
{
	uint32_t targetObjectId = PHX_GAMEOBJECT_NULL_ID;

	if (this)
		targetObjectId = this->ObjectId;

	Reflection::GenericValue gv{ targetObjectId };
	gv.Type = Reflection::ValueType::GameObject;

	return gv;
}

GameObject* GameObject::GetParent()
{
	if (this->Parent == PHX_GAMEOBJECT_NULL_ID)
		return nullptr;

	auto it = s_WorldArray.find(this->Parent);

	if (it == s_WorldArray.end())
	{
		this->Parent = PHX_GAMEOBJECT_NULL_ID;
		return nullptr;
	}
	else
		return it->second;
}

std::vector<GameObject*> GameObject::GetChildren()
{
	std::vector<GameObject*> children;
	children.reserve(m_Children.size());

	for (auto& childEntry : m_Children)
	{
		uint32_t childId = childEntry.second;

		GameObject* child = GameObject::GetObjectById(childId);

		if (child)
			children.push_back(child);
		else
			m_Children.erase(childEntry.first);
	}

	return children;
}

std::vector<GameObject*> GameObject::GetDescendants()
{
	std::vector<GameObject*> descendants;
	descendants.reserve(m_Children.size());

	for (auto& childEntry : m_Children)
	{
		uint32_t childId = childEntry.second;

		GameObject* child = GameObject::GetObjectById(childId);

		if (child)
		{
			descendants.push_back(child);

			std::vector<GameObject*> childrenDescendants = child->GetDescendants();
			std::copy(childrenDescendants.begin(), childrenDescendants.end(), std::back_inserter(descendants));
		}
		else
			m_Children.erase(childEntry.first);
	}

	return descendants;
}

GameObject* GameObject::GetChildById(uint32_t id)
{
	auto it = m_Children.find(id);

	return it != m_Children.end() ? s_WorldArray[id] : nullptr;
}

GameObject* GameObject::GetChild(std::string const& ChildName)
{
	for (auto& it : m_Children)
	{
		GameObject* child = GameObject::GetObjectById(it.second);

		if (!child)
		{
			m_Children.erase(it.first);
			continue;
		}

		if (child->Name == ChildName)
			return child;
	}

	return nullptr;
}


GameObject* GameObject::GetChildOfClass(std::string const& Class)
{
	for (auto& it : m_Children)
	{
		GameObject* child = this->GetChildById(m_Children[it.second]);

		if (!child)
		{
			m_Children.erase(it.first);
			continue;
		}

		if (child->ClassName == Class)
			return child;
	}

	return nullptr;
}

GameObject* GameObject::Create(std::string const& ObjectClass)
{
	GameObjectMapType::iterator it = s_GameObjectMap->find(ObjectClass);

	if (it == s_GameObjectMap->end())
		throw(std::vformat(
			"Attempted to create invalid GameObject '{}'!",
			std::make_format_args(ObjectClass)
		));

	GameObject* CreatedObject = it->second();
	// ID:0 is a reserved ID
	// Whenever anyone tries to use it, we know that an uninitialized
	// Object was involved.
	// To indicate a NULL Object, use `PHX_GAMEOBJECT_NULL_ID`.
	// Thus, add 1 so that the first create Object has ID 1 and not 0.
	CreatedObject->ObjectId = NumGameObjects + 1;
	NumGameObjects++;

	s_WorldArray.insert(std::pair(CreatedObject->ObjectId, CreatedObject));

	CreatedObject->Initialize();

	return CreatedObject;
}

nlohmann::json GameObject::DumpApiToJson()
{
	nlohmann::json dump{};
	
	dump["GameObject"] = nlohmann::json();

	for (auto& g : *s_GameObjectMap)
	{
		auto newobj = g.second();
		dump[g.first] = nlohmann::json();

		for (auto& p : newobj->GetProperties())
			dump[g.first][p.first] = Reflection::TypeAsString(p.second.Type);

		for (auto& f : newobj->GetFunctions())
		{
			std::string istring = "";
			std::string ostring = "";

			for (Reflection::ValueType i : f.second.Inputs)
				istring += Reflection::TypeAsString(i) + ",";

			for (Reflection::ValueType o : f.second.Outputs)
				ostring += Reflection::TypeAsString(o) + ",";

			dump[g.first][f.first] = std::vformat("({}) -> ({})", std::make_format_args(istring, ostring));
		}

		//delete newobj;
	}

	return dump;
}
