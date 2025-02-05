#include <tracy/Tracy.hpp>

#include "datatype/GameObject.hpp"
#include "Log.hpp"

PHX_GAMEOBJECT_LINKTOCLASS_SIMPLE(GameObject);

static bool s_DidInitReflection = false;

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

static void mergeRecursive(
	GameObject* me,
	GameObject* other,
	std::unordered_map<GameObject*, GameObject*>& MergedOverrides
)
{
	if (me->ClassName != other->ClassName)
		throw(std::vformat(
			"Tried to `:Merge` a {} with a {}",
			std::make_format_args(me->ClassName, other->ClassName)
		));

	MergedOverrides[other] = me;

	for (GameObject* ch : other->GetChildren())
		if (GameObject* og = me->FindChild(ch->Name))
			mergeRecursive(og, ch, MergedOverrides);
		else
			ch->SetParent(me);

	for (auto& it : other->GetProperties())
		if (it.second.Set && it.first != "Parent")
		{
			Reflection::GenericValue v = it.second.Get(other);

			if (v.Type == Reflection::ValueType::GameObject)
			{
				auto moit = MergedOverrides.find(GameObject::FromGenericValue(v));
				if (moit != MergedOverrides.end())
					v = moit->second->ToGenericValue();
			}

			me->SetPropertyValue(it.first, v);
		}

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
	REFLECTION_DECLAREPROP_SIMPLE(GameObject, Serializes, Bool);
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
		},
		[](Reflection::Reflectable* p, const Reflection::GenericValue& gv)
		{
			static_cast<GameObject*>(p)->SetParent(GameObject::FromGenericValue(gv));
		}
	);

	REFLECTION_DECLAREPROC_INPUTLESS(
		Destroy,
		[](Reflection::Reflectable* p)
		{
			static_cast<GameObject*>(p)->Destroy();
		}
	);
	REFLECTION_DECLAREFUNC(
		"GetFullName",
		{},
		{ Reflection::ValueType::String },
		[](Reflection::Reflectable* p, const std::vector<Reflection::GenericValue>&)
		-> std::vector<Reflection::GenericValue>
		{
			return { static_cast<GameObject*>(p)->GetFullName() };
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
			return { Reflection::GenericValue(retval) };
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
			return { Reflection::GenericValue(retval) };
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
		"Duplicate",
		{},
		{ Reflection::ValueType::GameObject },
		[](Reflection::Reflectable* p, const std::vector<Reflection::GenericValue>&)
		-> std::vector<Reflection::GenericValue>
		{
			GameObject* g = static_cast<GameObject*>(p);

			return { g->Duplicate()->ToGenericValue() };
		}
	);

	REFLECTION_DECLAREFUNC(
		"MergeWith",
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

			me->MergeWith(inputObject);

			return {};
		}
	);
}

GameObject::GameObject()
{
	s_DeclareReflections();
	ApiPointer = &s_Api;
}

GameObject* GameObject::Duplicate()
{
	ZoneScoped;
	return cloneRecursive(this);
}

void GameObject::MergeWith(GameObject* Other)
{
	ZoneScoped;

	if (Other->ClassName != this->ClassName)
		throw(std::vformat(
			"Tried to `:MergeWith` a {} with a {}",
			std::make_format_args(this->ClassName, Other->ClassName)
		));

	// not sure if i actually need to do this
	// 24/12/2024
	std::unordered_map<GameObject*, GameObject*> mergedOverridesDummy;

	mergeRecursive(this, Other, mergedOverridesDummy);

	Other->Destroy();
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

GameObject::~GameObject() noexcept(false)
{
	if (m_HardRefCount != 0)
		// use `::Destroy` or something maybe 24/12/2024
		throw("I can't be killed right now, someone still needs me!");

	for (GameObject* child : this->GetChildren())
		child->Destroy();

	if (this->ObjectId != PHX_GAMEOBJECT_NULL_ID)
		s_WorldArray.at(this->ObjectId) = nullptr;
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

static void validateRefCount(GameObject* Object, uint32_t RefCount)
{
	if (RefCount > 254)
	{
		std::string fullName = Object->GetFullName();
		throw(std::vformat(
			"Engine has reached Reference Count limit of 255 for Object '{}' (ID:{})",
			std::make_format_args(fullName, Object->ObjectId)
		));
	}
	// only kill ourselves if nobody cares about us (i'm so edgy) 24/12/2024
	else if (RefCount == 0)
		delete Object;
}

void GameObject::IncrementHardRefs()
{
	m_HardRefCount++;

	validateRefCount(this, m_HardRefCount);
}

void GameObject::DecrementHardRefs()
{
	if (m_HardRefCount == 0)
		// use `delete` directly instead maybe
		throw("Tried to decrement hard refs, with no hard references!");

	m_HardRefCount--;

	validateRefCount(this, m_HardRefCount);
}

void GameObject::Destroy()
{
	bool wasDestructionPending = this->IsDestructionPending;

	this->SetParent(nullptr);
	this->IsDestructionPending = true;

	if (!wasDestructionPending)
		DecrementHardRefs(); // removes the first ref in `GameObject::Create`
	else
		validateRefCount(this, m_HardRefCount);
}

std::string GameObject::GetFullName() const
{
	std::string fullName = this->Name;
	const GameObject* curObject = this;

	while (GameObject* parent = curObject->GetParent())
	{
		if (parent == GameObject::s_DataModel)
			break;
		fullName = parent->Name + "." + fullName;
		curObject = parent;
	}

	return fullName;
}

bool GameObject::IsA(const std::string& AncestorClass) const
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

static uint32_t NullGameObjectIdValue = PHX_GAMEOBJECT_NULL_ID;

void GameObject::SetParent(GameObject* newParent)
{
	std::string fullname = this->GetFullName();

	if (this->IsDestructionPending)
	{
		std::string parentFullName = newParent ? newParent->GetFullName() : "<NULL>";

		throw(std::vformat(
			"Tried to re-parent '{}' (ID:{}) to '{}' (ID:{}), but it's Parent has been locked due to `::Destroy`",
			std::make_format_args(
				fullname,
				this->ObjectId,
				parentFullName,
				newParent ? newParent->ObjectId : NullGameObjectIdValue
			)
		));
	}

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

		if (isOwnDescendant)
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

	GameObject* oldParent = GameObject::GetObjectById(Parent);

	if (newParent == oldParent)
		return;

	if (oldParent)
		oldParent->RemoveChild(this->ObjectId);

	if (!newParent)
	{
		this->Parent = PHX_GAMEOBJECT_NULL_ID;
		return;
	}

	this->Parent = newParent->ObjectId;
	newParent->AddChild(this);
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

Reflection::GenericValue GameObject::ToGenericValue() const
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
	ZoneScoped;

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

GameObject* GameObject::FindChild(const std::string& ChildName)
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


GameObject* GameObject::FindChildWhichIsA(const std::string& Class)
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

	const GameObjectMapType::iterator& it = s_GameObjectMap.find(ObjectClass);

	if (it == s_GameObjectMap.end())
		throw(std::vformat(
			"Attempted to create invalid GameObject '{}'!",
			std::make_format_args(ObjectClass)
		));

	// `it->second` is a function that constructs the object, so we
	// call it
	GameObject* CreatedObject = it->second();

	s_WorldArray.push_back(CreatedObject);
	CreatedObject->IncrementHardRefs(); // make it hard
	CreatedObject->ObjectId = numObjects;

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

		nlohmann::json& gapi = dump[g.first];

		const std::vector<std::string>& lineage = newobj->GetLineage();

		gapi["Lineage"] = "";

		for (size_t index = 0; index < lineage.size(); index++)
			if (index < lineage.size() - 1)
				gapi["Lineage"] = (std::string)gapi["Lineage"] + lineage[index] + " -> ";
			else
				gapi["Lineage"] = (std::string)gapi["Lineage"] + lineage[index];

		gapi["Properties"] = {};
		gapi["Functions"] = {};

		nlohmann::json& props = gapi["Properties"];
		nlohmann::json& funcs = gapi["Functions"];

		for (auto& p : newobj->GetProperties())
			props[p.first] = Reflection::TypeAsString(p.second.Type)
								+ ": "
								+ (p.second.Get ? "Read" : "")
								+ (p.second.Set ? " | Write" : "");

		for (auto& f : newobj->GetFunctions())
		{
			std::string istring = "";
			std::string ostring = "";

			for (Reflection::ValueType i : f.second.Inputs)
				istring += Reflection::TypeAsString(i) + ", ";

			for (Reflection::ValueType o : f.second.Outputs)
				ostring += Reflection::TypeAsString(o) + ", ";

			istring = istring.substr(0, istring.size() - 2);
			ostring = ostring.substr(0, ostring.size() - 2);

			funcs[f.first] = std::vformat("({}) -> ({})", std::make_format_args(istring, ostring));
		}

		delete newobj;
	}

	return dump;
}
