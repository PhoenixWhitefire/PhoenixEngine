#include <tracy/Tracy.hpp>

#include "datatype/GameObject.hpp"
#include "Log.hpp"

// https://stackoverflow.com/a/75891310
struct RefHasher
{
	size_t operator()(const GameObjectRef& a) const
    {
        return a.m_TargetId;
    }
};

static GameObject* cloneRecursive(
	GameObjectRef Root,
	// u_m < og-child, vector < pair < clone-object-referencing-ogchild, property-referencing-ogchild > > >
	std::unordered_map<GameObjectRef, std::vector<std::pair<GameObjectRef, std::string_view>>, RefHasher> OverwritesMap = {},
	std::unordered_map<GameObjectRef, GameObjectRef, RefHasher> OriginalToCloneMap = {}
)
{
	GameObjectRef newObj = GameObject::Create();

	for (const std::pair<EntityComponent, uint32_t>& pair : Root->GetComponents())
		newObj->AddComponent(pair.first);

	auto overwritesIt = OverwritesMap.find(Root);

	if (overwritesIt != OverwritesMap.end())
	{
		for (const std::pair<GameObjectRef, std::string_view>& overwrite : overwritesIt->second)
			// change the reference to the OG object to it's clone
			overwrite.first->SetPropertyValue(overwrite.second, newObj->ToGenericValue());

		overwritesIt->second.clear();
	}

	const std::vector<GameObject*> rootDescs = Root->GetDescendants();

	for (auto& it : Root->GetProperties())
	{
		if (!it.second.Set)
			continue; // read-only

		Reflection::GenericValue rootVal = Root->GetPropertyValue(it.first);

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

	std::vector<GameObjectRef> chrefs;

	for (GameObject* ch : Root->GetChildren())
		chrefs.emplace_back(ch);

	for (GameObjectRef ch : chrefs)
		cloneRecursive(ch, OverwritesMap, OriginalToCloneMap)->SetParent(newObj);

	return newObj;
}

static void mergeRecursive(
	GameObjectRef me,
	GameObjectRef other,
	std::unordered_map<uint32_t, uint32_t>& MergedOverrides
)
{
	MergedOverrides[other->ObjectId] = me->ObjectId;

	for (GameObjectRef ch : other->GetChildren())
		if (GameObject* og = me->FindChild(ch->Name))
			mergeRecursive(og, ch, MergedOverrides);
		else
			ch->SetParent(me);
	
	for (const std::pair<EntityComponent, uint32_t>& pair : other->GetComponents())
		if (!me->GetComponentByType(pair.first))
			me->AddComponent(pair.first);

	for (auto& it : other->GetProperties())
		if (it.second.Set && it.first != "Parent")
		{
			Reflection::GenericValue v = other->GetPropertyValue(it.first);

			if (v.Type == Reflection::ValueType::GameObject)
			{
				// TODO 29/05/2025
				// not sure what it means if this is NULL, i kinda forgor how this
				// whole thing works
				GameObject* g = GameObject::FromGenericValue(v);

				auto moit = MergedOverrides.find(g ? g->ObjectId : PHX_GAMEOBJECT_NULL_ID);
				if (moit != MergedOverrides.end())
					v = GameObject::GetObjectById(moit->second)->ToGenericValue();
			}

			me->SetPropertyValue(it.first, v);
		}

	for (GameObjectRef d : me->GetDescendants())
		for (auto& it : d->GetProperties())
		{
			Reflection::GenericValue v = d->GetPropertyValue(it.first);

			if (v.Type == Reflection::ValueType::GameObject && GameObject::FromGenericValue(v)->ObjectId == other->ObjectId)
				d->SetPropertyValue(it.first, me->ToGenericValue());
		}
}

void GameObject::s_AddObjectApi()
{
	static bool s_DidInitApi = false;

	if (s_DidInitApi)
		return;
	s_DidInitApi = true;

	REFLECTION_DECLAREPROP_SIMPSTR(GameObject, Name);
	REFLECTION_DECLAREPROP_SIMPLE(GameObject, Enabled, Boolean);
	REFLECTION_DECLAREPROP_SIMPLE(GameObject, Serializes, Boolean);
	REFLECTION_DECLAREPROP_SIMPLE_READONLY(GameObject, ObjectId, Integer);
	REFLECTION_DECLAREPROP(
		"Parent",
		GameObject,
		[](void* p)
		{
			// This is OK even if `->GetParent()` returns `nullptr`,
			// because `::ToGenericValue` accounts for when `this` is `nullptr`
			// 06/10/2024
			return static_cast<GameObject*>(p)->GetParent()->ToGenericValue();
		},
		[](void* p, const Reflection::GenericValue& gv)
		{
			static_cast<GameObject*>(p)->SetParent(GameObject::FromGenericValue(gv));
		}
	);

	REFLECTION_DECLAREPROC_INPUTLESS(
		Destroy,
		[](void* p)
		{
			static_cast<GameObject*>(p)->Destroy();
		}
	);
	REFLECTION_DECLAREFUNC(
		"GetFullName",
		{},
		{ Reflection::ValueType::String },
		[](void* p, const std::vector<Reflection::GenericValue>&)
		-> std::vector<Reflection::GenericValue>
		{
			return { static_cast<GameObject*>(p)->GetFullName() };
		}
	);

	REFLECTION_DECLAREFUNC(
		"GetChildren",
		{},
		{ Reflection::ValueType::Array },
		[](void* p, const std::vector<Reflection::GenericValue>&)
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
		[](void* p, const std::vector<Reflection::GenericValue>&)
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
		{ Reflection::ValueType::Boolean },
		[](void* p, const std::vector<Reflection::GenericValue>& gv)
		-> std::vector<Reflection::GenericValue>
		{
			std::string_view ancestor = gv[0].AsStringView();
			return { static_cast<GameObject*>(p)->IsA(ancestor) };
		}
	);

	REFLECTION_DECLAREFUNC(
		"Duplicate",
		{},
		{ Reflection::ValueType::GameObject },
		[](void* p, const std::vector<Reflection::GenericValue>&)
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
		[](void* p, const std::vector<Reflection::GenericValue>& inputs)
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

	REFLECTION_DECLAREFUNC(
		"FindChild",
		{ Reflection::ValueType::String },
		{ Reflection::ValueType::GameObject },
		[](void* p, const std::vector<Reflection::GenericValue>& inputs)
		-> std::vector<Reflection::GenericValue>
		{
			return { static_cast<GameObject*>(p)->FindChild(inputs.at(0).AsStringView())->ToGenericValue() };
		}
	);

	REFLECTION_DECLAREFUNC(
		"GetComponentNames",
		{},
		{ Reflection::ValueType::Array },
		[](void* p, const std::vector<Reflection::GenericValue>&)
		-> std::vector<Reflection::GenericValue>
		{
			std::vector<Reflection::GenericValue> ret;

			for (const std::pair<EntityComponent, uint32_t>& pair : static_cast<GameObject*>(p)->GetComponents())
				ret.emplace_back(s_EntityComponentNames[(size_t)pair.first]);
			
			return { ret };
		}
	);

	REFLECTION_DECLAREFUNC(
		"HasComponent",
		{ Reflection::ValueType::String },
		{ Reflection::ValueType::Boolean },
		[](void* p, const std::vector<Reflection::GenericValue>& inputs)
		-> std::vector<Reflection::GenericValue>
		{
			std::string_view requested = inputs.at(0).AsStringView();

			if (!GameObject::IsValidClass(requested))
				throw("Invalid component type");
			
			for (const std::pair<EntityComponent, uint32_t>& pair : static_cast<GameObject*>(p)->GetComponents())
				if (s_EntityComponentNames[(size_t)pair.first] == requested)
					return { true };
			
			return { false };
		}
	);
}

GameObject::GameObject()
{
	s_AddObjectApi();
}

GameObject* GameObject::Duplicate()
{
	ZoneScoped;
	return cloneRecursive(this);
}

void GameObject::MergeWith(GameObject* Other)
{
	ZoneScoped;

	// not sure if i actually need to do this
	// 24/12/2024
	std::unordered_map<uint32_t, uint32_t> mergedOverridesDummy;

	mergeRecursive(this, Other, mergedOverridesDummy);

	Other->Destroy();
}

GameObject* GameObject::FromGenericValue(const Reflection::GenericValue& gv)
{
	if (gv.Type == Reflection::ValueType::Null)
		return nullptr;

	if (gv.Type != Reflection::ValueType::GameObject)
		throw(std::format(
			"Tried to GameObject::FromGenericValue, but GenericValue had Type '{}' instead",
			Reflection::TypeAsString(gv.Type)
		));

	return GameObject::GetObjectById(static_cast<uint32_t>((int64_t)gv.Value));
}

bool GameObject::IsValidClass(const std::string_view& ObjectClass)
{
	for (size_t i = 0; i < (size_t)EntityComponent::__count; i++)
		if (s_EntityComponentNames[i] == ObjectClass)
			return true;

	return false;
}

GameObject* GameObject::GetObjectById(uint32_t Id)
{
	if (Id == PHX_GAMEOBJECT_NULL_ID || Id >= s_WorldArray.size())
		return nullptr;

	GameObject& obj = s_WorldArray[Id];
	
	return obj.Valid ? &obj : nullptr;
}

void* GameObject::ComponentHandleToPointer(const std::pair<EntityComponent, uint32_t>& Handle)
{
	return GameObject::s_ComponentManagers[(size_t)Handle.first]->GetComponent(Handle.second);
}

void GameObject::IncrementHardRefs()
{
	//m_HardRefCount++;

	//if (m_HardRefCount > UINT16_MAX - 1)
	//	throw("Too many hard refs!");
}

void GameObject::DecrementHardRefs()
{
	//if (m_HardRefCount == 0)
	//	throw("Tried to decrement hard refs, with no hard references!");

	//m_HardRefCount--;

	//if (m_HardRefCount == 0)
	//	Destroy();
}

void GameObject::Destroy()
{
	ZoneScoped;

	assert(Valid);

	if (!IsDestructionPending)
	{
		Valid = false;
		this->SetParent(nullptr);
		Valid = true; // TODO HACK to avoid RemoveChild -> DecrementHardRefs -> Destroy loop

		this->IsDestructionPending = true;

		if (m_HardRefCount > 0)
			DecrementHardRefs(); // removes the reference in `::Create`
	}

	if (m_HardRefCount == 0 && Valid)
	{
		for (GameObject* child : this->GetChildren())
			child->Destroy();

		Valid = false;

		for (const std::pair<EntityComponent, uint32_t>& pair : m_Components)
			s_ComponentManagers[(size_t)pair.first]->DeleteComponent(pair.second);
		
		m_Components.clear();
	}
}

std::string GameObject::GetFullName() const
{
	std::string fullName = this->Name;
	const GameObject* curObject = this;

	while (GameObject* parent = curObject->GetParent())
	{
		if (parent->ObjectId == GameObject::s_DataModel)
			break;
		fullName = parent->Name + "." + fullName;
		curObject = parent;
	}

	return fullName;
}

bool GameObject::IsA(const std::string_view& AncestorClass) const
{
	for (const std::pair<EntityComponent, uint32_t>& p : m_Components)
		if (s_EntityComponentNames[(size_t)p.first] == AncestorClass)
			return true;
	
	return false;
}

static uint32_t NullGameObjectIdValue = PHX_GAMEOBJECT_NULL_ID;

void GameObject::SetParent(GameObject* newParent)
{
	if (this->IsDestructionPending)
		throw(std::format(
			"Tried to re-parent '{}' (ID:{}) to '{}' (ID:{}), but it's Parent has been locked due to `::Destroy`",

			GetFullName(),
			this->ObjectId,
			newParent ? newParent->GetFullName() : "<NULL>",
			newParent ? newParent->ObjectId : NullGameObjectIdValue
		));
	
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
			throw(std::format(
				"Tried to make object ID:{} ('{}') a descendant of itself",
				this->ObjectId, GetFullName()
			));
	}
	else
		throw(std::format(
			"Tried to make object ID:{} ('{}') it's own parent",
			this->ObjectId, GetFullName()
		));

	GameObject* oldParent = GetObjectById(Parent);

	if (newParent == oldParent)
		return;

	// we HAVE to do this BEFORE `::RemoveChild`, otherwise
	// it could get called twice in a row due to `::DecrementHardRefs`
	// leading to a `::Destroy`
	Parent = PHX_GAMEOBJECT_NULL_ID;

	if (oldParent)
		oldParent->RemoveChild(this->ObjectId);

	if (!newParent)
		return;

	this->Parent = newParent->ObjectId;
	newParent->AddChild(this);
}

void GameObject::AddChild(GameObject* c)
{
	if (c->ObjectId == this->ObjectId)
		throw(std::format(
			"::AddChild called on Object ID:{} (`{}`) with itself as the adopt'ed",
			this->ObjectId, GetFullName()
		));

	auto it = std::find(m_Children.begin(), m_Children.end(), c->ObjectId);

	if (it == m_Children.end())
	{
		m_Children.push_back(c->ObjectId);
		c->IncrementHardRefs();
	}
}

void GameObject::RemoveChild(uint32_t id)
{
	auto it = std::find(m_Children.begin(), m_Children.end(), id);

	if (it != m_Children.end())
	{
		m_Children.erase(it);

		// TODO HACK check ::Destroy
		if (GameObject* ch = GameObject::GetObjectById(id))
			ch->DecrementHardRefs();
	}
	else
		throw(std::format("ID:{} is _not my ({}) sonnn~_", ObjectId, id));
}

Reflection::GenericValue GameObject::s_ToGenericValue(const GameObject* Object)
{
	if (Object)
	{
		Reflection::GenericValue gv{ Object->ObjectId };
		gv.Type = Reflection::ValueType::GameObject;

		return gv;
	}
	else
		return {}; // null
}

Reflection::GenericValue GameObject::ToGenericValue() const
{
	return s_ToGenericValue(this);
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

		if (child && child->Valid && !child->IsDestructionPending)
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

GameObject* GameObject::FindChild(const std::string_view& ChildName)
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


GameObject* GameObject::FindChildWhichIsA(const std::string_view& Class)
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

uint32_t GameObject::AddComponent(EntityComponent Type)
{
	PHX_ENSURE(Valid);

	for (const std::pair<EntityComponent, uint32_t>& pair : m_Components)
		if (pair.first == Type)
			throw("Already have that component");
	
	BaseComponentManager* manager = GameObject::s_ComponentManagers[(size_t)Type];
	m_Components.emplace_back(Type, manager->CreateComponent(this));

	uint32_t componentId = m_Components.back().second;

	for (const auto& it : manager->GetProperties())
	{
		m_ComponentApis.Properties[it.first] = it.second;
		m_MemberToComponentMap[it.first] = m_Components.back();
	}
	for (const auto& it : manager->GetFunctions())
	{
		m_ComponentApis.Functions[it.first] = it.second;
		m_MemberToComponentMap[it.first] = m_Components.back();
	}

	return componentId;
}

void GameObject::RemoveComponent(EntityComponent Type)
{
	for (auto it = m_Components.begin(); it < m_Components.end(); it++)
		if (it->first == Type)
		{
			m_Components.erase(it);

			BaseComponentManager* manager = GameObject::s_ComponentManagers[(size_t)Type];
			manager->DeleteComponent(it->second);

			for (const auto& it2 : manager->GetProperties())
			{
				m_ComponentApis.Properties.erase(it2.first);
				m_MemberToComponentMap.erase(it2.first);
			}
			for (const auto& it2 : manager->GetFunctions())
			{
				m_ComponentApis.Functions.erase(it2.first);
				m_MemberToComponentMap.erase(it2.first);
			}
		}
	
	throw("Don't have that component");
}

Reflection::Property* GameObject::FindProperty(const std::string_view& PropName, std::pair<EntityComponent, uint32_t>* FromComponent)
{
	std::pair<EntityComponent, uint32_t> dummyFc;
	FromComponent = FromComponent ? FromComponent : &dummyFc;

	if (auto it = s_Api.Properties.find(PropName); it != s_Api.Properties.end())
	{
		return &it->second;
	}

	if (auto it = m_ComponentApis.Properties.find(PropName); it != m_ComponentApis.Properties.end())
	{
		*FromComponent = m_MemberToComponentMap[PropName];
		return &it->second;
	}

	return nullptr;
}
Reflection::Function* GameObject::FindFunction(const std::string_view& FuncName, std::pair<EntityComponent, uint32_t>* FromComponent)
{
	std::pair<EntityComponent, uint32_t> dummyFc;
	FromComponent = FromComponent ? FromComponent : &dummyFc;

	if (auto it = s_Api.Functions.find(FuncName); it != s_Api.Functions.end())
	{
		return &it->second;
	}

	if (auto it = m_ComponentApis.Functions.find(FuncName); it != m_ComponentApis.Functions.end())
	{
		*FromComponent = m_MemberToComponentMap[FuncName];
		return &it->second;
	}

	return nullptr;
}

Reflection::GenericValue GameObject::GetPropertyValue(const std::string_view& PropName)
{
	std::pair<EntityComponent, uint32_t> fromComponent;

	if (Reflection::Property* prop = FindProperty(PropName, &fromComponent))
	{
		if (fromComponent.first != EntityComponent::None)
			return prop->Get(ComponentHandleToPointer(m_MemberToComponentMap[PropName]));
		else
			return prop->Get(this);
	}

	throw("Invalid property in GetPropertyValue: " + std::string(PropName));
}
void GameObject::SetPropertyValue(const std::string_view& PropName, const Reflection::GenericValue& Value)
{
	std::pair<EntityComponent, uint32_t> fromComponent;

	if (Reflection::Property* prop = FindProperty(PropName, &fromComponent))
	{
		if (fromComponent.first != EntityComponent::None)
			prop->Set(ComponentHandleToPointer(m_MemberToComponentMap[PropName]), Value);
		else
			prop->Set(this, Value);
		
		return;
	}

	throw("Invalid property in SetPropertyValue: " + std::string(PropName));
}

std::vector<Reflection::GenericValue> GameObject::CallFunction(const std::string_view& FuncName, const std::vector<Reflection::GenericValue>& Inputs)
{
	std::pair<EntityComponent, uint32_t> fromComponent;

	if (Reflection::Function* func = FindFunction(FuncName, &fromComponent))
	{
		if (fromComponent.first != EntityComponent::None)
			return func->Func(ComponentHandleToPointer(m_MemberToComponentMap[FuncName]), Inputs);
		else
			return func->Func(this, Inputs);
	}

	throw("Invalid function in CallFunction: " + std::string(FuncName));
}

Reflection::PropertyMap GameObject::GetProperties() const
{
	// base APIs always take priority for consistency
	Reflection::PropertyMap cumulativeProps = m_ComponentApis.Properties;
	cumulativeProps.insert(s_Api.Properties.begin(), s_Api.Properties.end());

	return cumulativeProps;
}
Reflection::FunctionMap GameObject::GetFunctions() const
{
	Reflection::FunctionMap cumulativeFuncs = m_ComponentApis.Functions;
	cumulativeFuncs.insert(s_Api.Functions.begin(), s_Api.Functions.end());

	return cumulativeFuncs;
}

std::vector<std::pair<EntityComponent, uint32_t>>& GameObject::GetComponents()
{
	return m_Components;
}

void* GameObject::GetComponentByType(EntityComponent Type)
{
	for (const std::pair<EntityComponent, uint32_t>& pair : m_Components)
		if (pair.first == Type)
			return GameObject::s_ComponentManagers[(size_t)Type]->GetComponent(pair.second);
	
	return nullptr;
}

GameObject* GameObject::Create()
{
	uint32_t numObjects = static_cast<uint32_t>(s_WorldArray.size());

	if (numObjects >= UINT32_MAX - 1)
		throw("Reached end of GameObject ID space (2^32 - 1)");

#ifndef NDEBUG

	// cause as many re-allocations as possible to catch stale pointers
	//if (s_WorldArray.size() > 15)
	//	s_WorldArray.shrink_to_fit();
	// cant be bothered 29/05/2025

#endif

	s_WorldArray.emplace_back();
	GameObject& created = s_WorldArray.back();

	created.ObjectId = numObjects;
	created.IncrementHardRefs(); // "i'm tired boss"

	return &created;
}

GameObject* GameObject::Create(EntityComponent FirstComponent)
{
	GameObject* created = GameObject::Create();
	created->AddComponent(FirstComponent);
	created->Name = s_EntityComponentNames[(size_t)FirstComponent];

	return created;
}

GameObject* GameObject::Create(const std::string_view& FirstComponent)
{
	if (FirstComponent == "Primitive")
		return GameObject::Create("Mesh");

	auto it = s_ComponentNameToType.find(FirstComponent);

	if (it == s_ComponentNameToType.end())
		throw("Invalid Component Name " + std::string(FirstComponent));
	else
		return GameObject::Create(it->second);
}

static void dumpProperties(const Reflection::PropertyMap& Properties, nlohmann::json& Json)
{
	for (const auto& propIt : Properties)
		if (propIt.second.Set)
			Json[propIt.first] = Reflection::TypeAsString(propIt.second.Type);
		else
			Json[propIt.first] = std::string(Reflection::TypeAsString(propIt.second.Type)) + " READONLY";
}

static void dumpFunctions(const Reflection::FunctionMap& Functions, nlohmann::json& Json)
{
	for (const auto& funcIt : Functions)
	{
		std::string istring = "";
		std::string ostring = "";

		for (Reflection::ValueType i : funcIt.second.Inputs)
			istring += std::string(Reflection::TypeAsString(i)) + ", ";
		
		for (Reflection::ValueType o : funcIt.second.Outputs)
			ostring += std::string(Reflection::TypeAsString(o)) + ", ";
		
		istring = istring.substr(0, istring.size() - 2);
		ostring = ostring.substr(0, ostring.size() - 2);

		Json[funcIt.first] = std::format("({}) -> ({})", istring, ostring);
	}
}

nlohmann::json GameObject::DumpApiToJson()
{
	nlohmann::json dump;
	
	nlohmann::json& gameObjectApi = dump["Base"];
	nlohmann::json& componentApi = dump["Components"];

	nlohmann::json& gameObjectProperties = gameObjectApi["Properties"];
	nlohmann::json& gameObjectFunctions = gameObjectApi["Functions"];

	s_AddObjectApi(); // make sure we have the api
	dumpProperties(s_Api.Properties, gameObjectProperties);
	dumpFunctions(s_Api.Functions, gameObjectFunctions);
	
	for (size_t i = 0; i < (size_t)EntityComponent::__count; i++)
	{
		BaseComponentManager* manager = s_ComponentManagers[i];

		if (!manager)
			continue;

		nlohmann::json& api = componentApi[s_EntityComponentNames[i]];
		nlohmann::json& properties = api["Properties"];
		nlohmann::json& functions = api["Functions"];

		dumpProperties(manager->GetProperties(), properties);
		dumpFunctions(manager->GetFunctions(), functions);
	}

	return dump;
}
