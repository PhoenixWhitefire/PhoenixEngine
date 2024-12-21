#include "datatype/GameObject.hpp"
#include "Debug.hpp"

PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(GameObject);

static bool s_DidInitReflection = false;

static void destroyObject(Reflection::Reflectable* obj)
{
	static_cast<GameObject*>(obj)->Destroy();
}

static std::string getFullName(Reflection::Reflectable* r)
{
	GameObject* object = static_cast<GameObject*>(r);

	std::string fullName = object->Name;
	GameObject* curObject = object;

	while (GameObject* parent = curObject->GetParent())
	{
		if (parent == GameObject::s_DataModel)
			break;
		fullName = parent->Name + "." + fullName;
		curObject = parent;
	}

	return fullName;
}

static GameObject* cloneRecursive(
	GameObject* Root,
	// u_m < og-child, vector < pair < clone-object-referencing-ogchild, property-referencing-ogchild > > >
	std::unordered_map<GameObject*, std::vector<std::pair<GameObject*, std::string>>> OverwritesMap = {},
	std::unordered_map<GameObject*, GameObject*> OriginalToCloneMap = {}
)
{
	GameObject* newObj = GameObject::Create(Root->ClassName);

	auto overwritesIt = OverwritesMap.find(Root);

	if (overwritesIt != OverwritesMap.end())
	{
		for (const std::pair<GameObject*, std::string>& overwrite : overwritesIt->second)
			// change the reference to the OG object to it's clone
			overwrite.first->SetPropertyValue(overwrite.second, newObj->ToGenericValue());

		overwritesIt->second.clear();
	}

	const std::vector<GameObject*> rootDescs = Root->GetDescendants();

	for (auto& it : Root->GetProperties())
	{
		if (!it.second.Set)
			continue; // read-only

		Reflection::GenericValue rootVal = it.second.Get(Root);

		if (rootVal.Type == Reflection::ValueType::GameObject)
		{
			GameObject* ref = GameObject::FromGenericValue(rootVal);

			auto otcit = OriginalToCloneMap.find(ref);

			if (otcit != OriginalToCloneMap.end())
				newObj->SetPropertyValue(it.first, otcit->second->ToGenericValue());

			else
				if (ref && std::find(rootDescs.begin(), rootDescs.end(), ref) != rootDescs.end())
					OverwritesMap[ref].push_back(std::pair(newObj, it.first));
		}

		newObj->SetPropertyValue(it.first, rootVal);
	}

	for (GameObject* ch : Root->GetChildren())
		cloneRecursive(ch, OverwritesMap, OriginalToCloneMap)->SetParent(newObj);

	return newObj;
}

static void mergeCopyProps(GameObject* me, GameObject* other)
{
	for (auto& it : other->GetProperties())
		if (it.second.Set && it.first != "Parent")
			me->SetPropertyValue(it.first, it.second.Get(other));
}

static void mergeRecursive(GameObject* me, GameObject* other)
{
	if (me->ClassName != other->ClassName)
		throw(std::vformat(
			"Tried to `:Merge` a {} with a {}",
			std::make_format_args(me->ClassName, other->ClassName)
		));

	mergeCopyProps(me, other);

	for (GameObject* ch : other->GetChildren())
		if (GameObject* og = me->GetChild(ch->Name))
			mergeRecursive(og, ch);
		else
			ch->SetParent(me);

	for (GameObject* d : me->GetDescendants())
		for (auto& it : d->GetProperties())
		{
			Reflection::GenericValue v = d->GetPropertyValue(it.first);

			if (v.Type == Reflection::ValueType::GameObject && GameObject::FromGenericValue(v) == other)
				d->SetPropertyValue(it.first, me->ToGenericValue());
		}
}

void GameObject::s_DeclareReflections()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	REFLECTION_DECLAREPROP_SIMPLE_READONLY(GameObject, ClassName, String);

	// "Class" better
	// 22/10/2024
	s_Api.Properties["Class"] = s_Api.Properties["ClassName"];
	s_Api.Properties.erase("ClassName");

	REFLECTION_DECLAREPROP_SIMPLE(GameObject, Name, String);
	REFLECTION_DECLAREPROP_SIMPLE(GameObject, Enabled, Bool);
	REFLECTION_DECLAREPROP_SIMPLE_READONLY(GameObject, ObjectId, Integer);
	REFLECTION_DECLAREPROP(
		"Parent",
		GameObject,
		[](Reflection::Reflectable* p)
		{
			// This is OK even if `->GetParent()` returns `nullptr`,
			// because `::ToGenericValue` accounts for when `this` is `nullptr`
			// 06/10/2024
			return static_cast<GameObject*>(p)->GetParent()->ToGenericValue();
			/*
			Reflection::GenericValue gv = p->GetParent() ? p->GetParent()->ObjectId : PHX_GAMEOBJECT_NULL_ID;
			gv.Type = Reflection::ValueType::GameObject;
			return gv;
			*/
		},
		[](Reflection::Reflectable* p, const Reflection::GenericValue& gv)
		{
			GameObject* newParent = GameObject::GetObjectById(static_cast<uint32_t>(gv.AsInteger()));
			static_cast<GameObject*>(p)->SetParent(newParent);
		}
	);

	REFLECTION_DECLAREPROC_INPUTLESS(Destroy, destroyObject);
	REFLECTION_DECLAREFUNC(
		"GetFullName",
		{},
		{ Reflection::ValueType::String },
		[](Reflection::Reflectable* p, const std::vector<Reflection::GenericValue>&)
		-> std::vector<Reflection::GenericValue>
		{
			return { getFullName(p) };
		}
	);

	REFLECTION_DECLAREFUNC(
		"GetChildren",
		{},
		{ Reflection::ValueType::Array },
		[](Reflection::Reflectable* p, const std::vector<Reflection::GenericValue>&)
		-> std::vector<Reflection::GenericValue>
		{
			std::vector<Reflection::GenericValue> retval;
			for (GameObject* g : static_cast<GameObject*>(p)->GetChildren())
				retval.push_back(g->ToGenericValue());

			// ctor for ValueType::Array
			return { Reflection::GenericValue::GenericValue(retval) };
		}
	);

	REFLECTION_DECLAREFUNC(
		"GetDescendants",
		{},
		{ Reflection::ValueType::Array },
		[](Reflection::Reflectable* p, const std::vector<Reflection::GenericValue>&)
		-> std::vector<Reflection::GenericValue>
		{
			std::vector<Reflection::GenericValue> retval;
			for (GameObject* g : static_cast<GameObject*>(p)->GetDescendants())
				retval.push_back(g->ToGenericValue());

			// ctor for ValueType::Array
			return { Reflection::GenericValue::GenericValue(retval) };
		}
	);

	REFLECTION_DECLAREFUNC(
		"IsA",
		{ Reflection::ValueType::String },
		{ Reflection::ValueType::Bool },
		[](Reflection::Reflectable* p, const std::vector<Reflection::GenericValue>& gv)
		-> std::vector<Reflection::GenericValue>
		{
			std::string ancestor = gv[0].AsString();
			return { static_cast<GameObject*>(p)->IsA(ancestor) };
		}
	);

	REFLECTION_DECLAREFUNC(
		"Clone",
		{},
		{ Reflection::ValueType::GameObject },
		[](Reflection::Reflectable* p, const std::vector<Reflection::GenericValue>&)
		-> std::vector<Reflection::GenericValue>
		{
			GameObject* g = static_cast<GameObject*>(p);
			GameObject* newObj = cloneRecursive(g);

			return { newObj->ToGenericValue() };
		}
	);

	REFLECTION_DECLAREFUNC(
		"Merge",
		{ Reflection::ValueType::GameObject },
		{},
		[](Reflection::Reflectable* p, const std::vector<Reflection::GenericValue>& inputs)
		-> std::vector<Reflection::GenericValue>
		{
			// copy everything over from the input object
			// objects with the same name will have their properties
			// copied over

			GameObject* inputObject = GameObject::FromGenericValue(inputs.at(0));
			GameObject* me = static_cast<GameObject*>(p);

			if (inputObject->ClassName != me->ClassName)
				throw(std::vformat(
					"Tried to `:Merge` a {} with a {}",
					std::make_format_args(me->ClassName, inputObject->ClassName)
				));

			mergeRecursive(me, inputObject);

			//inputObject->Destroy();

			return {};
		}
	);

	//REFLECTION_INHERITAPI(Reflection::Reflectable);
}

GameObject::GameObject()
{
	s_DeclareReflections();
	ApiPointer = &s_Api;
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

	if (this->ObjectId != PHX_GAMEOBJECT_NULL_ID)
		s_WorldArray.at(this->ObjectId) = nullptr;

	this->Parent = PHX_GAMEOBJECT_NULL_ID;
	//this->ObjectId = PHX_GAMEOBJECT_NULL_ID;
	this->ParentLocked = true;
}

bool GameObject::IsValidObjectClass(const std::string& ObjectClass)
{
	GameObjectMapType::iterator it = s_GameObjectMap.find(ObjectClass);

	if (it == s_GameObjectMap.end())
		return false;
	else
		return true;
}

GameObject* GameObject::GetObjectById(uint32_t Id)
{
	if (Id == PHX_GAMEOBJECT_NULL_ID || s_WorldArray.size() - 1 < Id)
		return nullptr;

	return s_WorldArray.at(Id);
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

bool GameObject::IsA(const std::string& AncestorClass)
{
	if (this->ClassName == AncestorClass)
		return true;

	// we do `GetLineage` instead of `s_Api.Lineage` because
	// the latter refers to the lineage of `GameObject` (nothing as of 25/10/2024),
	// whereas the former is a virtual function of the class we actually are
	for (const std::string& ancestor : GetLineage())
		if (ancestor == AncestorClass)
			return true;
	return false;
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
		std::string fullname = this->GetFullName();

		if (newParent != this)
		{
			std::vector<GameObject*> descendants = this->GetDescendants();

			bool isOwnDescendant = false;

			for (GameObject* d : descendants)
				if (d == newParent)
				{
					isOwnDescendant = true;
					break;
				}

			if (!isOwnDescendant)
				this->Parent = newParent->ObjectId;
			else
				throw(std::vformat(
					"Tried to make object ID:{} ('{}') a descendant of itself",
					std::make_format_args(this->ObjectId, fullname)
				));
		}
		else
			throw(std::vformat(
				"Tried to make object ID:{} ('{}') it's own parent",
				std::make_format_args(this->ObjectId, fullname)
			));

		this->Parent = newParent->ObjectId;
		newParent->AddChild(this);
	}

	if (oldParent)
		oldParent->RemoveChild(this->ObjectId);
}

void GameObject::AddChild(GameObject* c)
{
	if (c->ObjectId == this->ObjectId)
	{
		std::string fullName = this->GetFullName();

		throw(std::vformat(
			"::AddChild called on Object ID:{} (`{}`) with itself as the adopt'ed",
			std::make_format_args(this->ObjectId, fullName)
		));
	}

	auto it = std::find(m_Children.begin(), m_Children.end(), c->ObjectId);

	if (it == m_Children.end())
		m_Children.push_back(c->ObjectId);
}

void GameObject::RemoveChild(uint32_t id)
{
	auto it = std::find(m_Children.begin(), m_Children.end(), id);

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

GameObject* GameObject::GetParent() const
{
	return GameObject::GetObjectById(this->Parent);
}

std::vector<GameObject*> GameObject::GetChildren()
{
	std::vector<GameObject*> children;
	children.reserve(m_Children.size());

	for (uint32_t index = 0; index < m_Children.size(); index++)
	{
		GameObject* child = GameObject::GetObjectById(m_Children[index]);

		if (child)
			children.push_back(child);
		else
			m_Children.erase(m_Children.begin() + index);
	}

	return children;
}

std::vector<GameObject*> GameObject::GetDescendants()
{
	std::vector<GameObject*> descendants;
	descendants.reserve(m_Children.size());

	for (uint32_t index = 0; index < m_Children.size(); index++)
	{
		GameObject* child = GameObject::GetObjectById(m_Children[index]);

		if (child)
		{
			descendants.push_back(child);

			std::vector<GameObject*> childrenDescendants = child->GetDescendants();
			std::copy(childrenDescendants.begin(), childrenDescendants.end(), std::back_inserter(descendants));
		}
		else
			m_Children.erase(m_Children.begin() + index);
	}

	return descendants;
}

GameObject* GameObject::GetChildById(uint32_t id)
{
	auto it = std::find(m_Children.begin(), m_Children.end(), id);

	return it != m_Children.end() ? GameObject::GetObjectById(id) : nullptr;
}

GameObject* GameObject::GetChild(const std::string& ChildName)
{
	for (uint32_t index = 0; index < m_Children.size(); index++)
	{
		GameObject* child = GameObject::GetObjectById(m_Children[index]);

		if (!child)
		{
			m_Children.erase(m_Children.begin() + index);
			continue;
		}

		if (child->Name == ChildName)
			return child;
	}

	return nullptr;
}


GameObject* GameObject::GetChildOfClass(const std::string& Class)
{
	for (uint32_t index = 0; index < m_Children.size(); index++)
	{
		GameObject* child = GameObject::GetObjectById(m_Children[index]);

		if (!child)
		{
			m_Children.erase(m_Children.begin() + index);
			continue;
		}

		if (child->IsA(Class))
			return child;
	}

	return nullptr;
}

GameObject* GameObject::Create(const std::string& ObjectClass)
{
	uint32_t numObjects = static_cast<uint32_t>(s_WorldArray.size());

	if (numObjects >= UINT32_MAX - 1)
		throw("Reached end of GameObject ID space (UINT32_MAX - 1)");

	GameObjectMapType::iterator it = s_GameObjectMap.find(ObjectClass);

	if (it == s_GameObjectMap.end())
		throw(std::vformat(
			"Attempted to create invalid GameObject '{}'!",
			std::make_format_args(ObjectClass)
		));

	// `it->second` is a function that constructs the object, so we
	// call it
	GameObject* CreatedObject = it->second();
	CreatedObject->ObjectId = numObjects;
	s_WorldArray.push_back(CreatedObject);

	CreatedObject->Initialize();

	return CreatedObject;
}

nlohmann::json GameObject::DumpApiToJson()
{
	nlohmann::json dump{};
	
	dump["GameObject"] = nlohmann::json();

	for (auto& g : s_GameObjectMap)
	{
		GameObject* newobj = g.second();
		dump[g.first] = nlohmann::json();

		const std::vector<std::string>& lineage = newobj->GetLineage();

		dump[g.first]["Lineage"] = "";

		for (size_t index = 0; index < lineage.size(); index++)
			if (index < lineage.size() - 1)
				dump[g.first]["Lineage"] = dump[g.first].value("Lineage", "") + lineage[index] + " -> ";
			else
				dump[g.first]["Lineage"] = dump[g.first].value("Lineage", "") + lineage[index];

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
