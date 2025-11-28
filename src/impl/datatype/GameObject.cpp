#include <tracy/Tracy.hpp>

#include "datatype/GameObject.hpp"
#include "component/Transform.hpp"
#include "component/Workspace.hpp"
#include "Log.hpp"

// https://stackoverflow.com/a/75891310
struct RefHasher
{
	size_t operator()(const ObjectRef& a) const
    {
        return a.TargetId;
    }
};

void* ReflectorRef::Referred() const
{
	if (Type == EntityComponent::None)
		return (void*)GameObject::GetObjectById(Id);
	else
		return GameObject::s_ComponentManagers[(size_t)Type]->GetComponent(Id);
}

static GameObject* cloneRecursive(
	ObjectRef Root,
	// u_m < og-child, vector < pair < clone-object-referencing-ogchild, property-referencing-ogchild > > >
	std::unordered_map<ObjectRef, std::vector<std::pair<ObjectRef, std::string_view>>, RefHasher> OverwritesMap = {},
	std::unordered_map<ObjectRef, ObjectRef, RefHasher> OriginalToCloneMap = {}
)
{
	ObjectRef newObj = GameObject::Create();

	for (const ReflectorRef& ref : Root->Components)
		newObj->AddComponent(ref.Type);

	auto overwritesIt = OverwritesMap.find(Root);

	if (overwritesIt != OverwritesMap.end())
	{
		for (const std::pair<ObjectRef, std::string_view>& overwrite : overwritesIt->second)
			// change the reference to the OG object to it's clone
			overwrite.first->SetPropertyValue(overwrite.second, newObj->ToGenericValue());

		overwritesIt->second.clear();
	}

	const std::vector<GameObject*> rootDescs = Root->GetDescendants();

	for (auto& it : Root->GetProperties())
	{
		if (!it.second->Set)
			continue; // read-only

		Reflection::GenericValue rootVal = Root->GetPropertyValue(it.first);

		if (rootVal.Type == Reflection::ValueType::GameObject)
		{
			GameObject* ref = GameObject::FromGenericValue(rootVal);
			if (!ref)
				continue;

			auto otcit = OriginalToCloneMap.find(ref);

			if (otcit != OriginalToCloneMap.end())
				newObj->SetPropertyValue(it.first, otcit->second->ToGenericValue());

			else
				if (ref && std::find(rootDescs.begin(), rootDescs.end(), ref) != rootDescs.end())
					OverwritesMap[ref].push_back(std::pair(newObj, it.first));
		}

		newObj->SetPropertyValue(it.first, rootVal);
	}

	std::vector<ObjectRef> chrefs;
	for (GameObject* ch : Root->GetChildren())
		chrefs.emplace_back(ch);

	for (ObjectRef ch : chrefs)
	{
		if (ch->Serializes)
			cloneRecursive(ch, OverwritesMap, OriginalToCloneMap)->SetParent(newObj);
	}

	return newObj;
}

static void mergeRecursive(
	ObjectRef me,
	ObjectRef other,
	std::unordered_map<uint32_t, uint32_t>& MergedOverrides
)
{
	MergedOverrides[other->ObjectId] = me->ObjectId;

	for (ObjectRef ch : other->GetChildren())
		if (GameObject* og = me->FindChild(ch->Name))
			mergeRecursive(og, ch, MergedOverrides);
		else
			ch->SetParent(me);
	
	for (const ReflectorRef& ref : other->Components)
		if (!me->FindComponentByType(ref.Type))
			me->AddComponent(ref.Type);

	for (auto& it : other->GetProperties())
		if (it.second->Set && it.first != "Parent")
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

	for (ObjectRef d : me->GetDescendants())
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
		"Exists",
		Boolean,
		[](void* p) -> Reflection::GenericValue
		{
			return true; // actual logic is handled in `api_gameobjindex`
		},
		nullptr
	);
	s_Api.Properties["Parent"] = Reflection::PropertyDescriptor
	{
		REFLECTION_OPTIONAL(GameObject),
		[](void* p)
		{
			return static_cast<GameObject*>(p)->GetParent()->ToGenericValue();
		},
		[](void* p, const Reflection::GenericValue& gv)
		{
			static_cast<GameObject*>(p)->SetParent(GameObject::FromGenericValue(gv));
		}
	};

	REFLECTION_DECLAREFUNC(
		"Destroy",
		{},
		{},
		[](void* p, const std::vector<Reflection::GenericValue>&)
		-> std::vector<Reflection::GenericValue>
		{
			static_cast<GameObject*>(p)->Destroy();
			return {};
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
		"ForEachChild",
		{ Reflection::ValueType::Function },
		{},
		[](void* p, const std::vector<Reflection::GenericValue>& gv)
		-> std::vector<Reflection::GenericValue>
		{
			const Reflection::GenericFunction& gf = gv.at(0).AsFunction(); // damn

			static_cast<GameObject*>(p)->ForEachChild(
				// damn
				[gf](GameObject* g)
				-> bool
				{
					std::vector<Reflection::GenericValue> rets = gf({ g->ToGenericValue() }); // damn
					PHX_ENSURE_MSG(rets.size() <= 1, "`:ForEachChild` expects none or one return value");

					if (rets.size() == 0)
						return true;
					else
						return rets[0].AsBoolean();
				}
			);

			return {};
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
		{ REFLECTION_OPTIONAL(GameObject) },
		[](void* p, const std::vector<Reflection::GenericValue>& inputs)
		-> std::vector<Reflection::GenericValue>
		{
			return { static_cast<GameObject*>(p)->FindChild(inputs.at(0).AsStringView())->ToGenericValue() };
		}
	);

	REFLECTION_DECLAREFUNC(
		"FindChildWithComponent",
		{ Reflection::ValueType::String },
		{ REFLECTION_OPTIONAL(GameObject) },
		[](void* p, const std::vector<Reflection::GenericValue>& inputs)
		-> std::vector<Reflection::GenericValue>
		{
			const auto& it = s_ComponentNameToType.find(inputs[0].AsStringView());
			if (it == s_ComponentNameToType.end())
				RAISE_RTF("Invalid component type '{}'", inputs[0].AsStringView());

			return { static_cast<GameObject*>(p)->FindChildWithComponent(it->second)->ToGenericValue() };
		}
	);

	REFLECTION_DECLAREFUNC(
		"GetComponents",
		{},
		{ Reflection::ValueType::Array },
		[](void* p, const std::vector<Reflection::GenericValue>&)
		-> std::vector<Reflection::GenericValue>
		{
			std::vector<Reflection::GenericValue> ret;

			for (const ReflectorRef& ref : static_cast<GameObject*>(p)->Components)
				ret.emplace_back(s_EntityComponentNames[(size_t)ref.Type]);
			
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
				RAISE_RT("Invalid component");
			
			for (const ReflectorRef& ref : static_cast<GameObject*>(p)->Components)
				if (s_EntityComponentNames[(size_t)ref.Type] == requested)
					return { true };
			
			return { false };
		}
	);

	REFLECTION_DECLAREFUNC(
		"AddComponent",
		{ Reflection::ValueType::String },
		{},
		[](void* p, const std::vector<Reflection::GenericValue>& inputs)
		-> std::vector<Reflection::GenericValue>
		{
			std::string_view requested = inputs.at(0).AsStringView();
			const auto& it = s_ComponentNameToType.find(requested);

			if (it == s_ComponentNameToType.end())
				RAISE_RT("Invalid component");

			GameObject* obj = static_cast<GameObject*>(p);
			obj->AddComponent(it->second);

			return {};
		}
	);

	REFLECTION_DECLAREFUNC(
		"RemoveComponent",
		{ Reflection::ValueType::String },
		{},
		[](void* p, const std::vector<Reflection::GenericValue>& inputs)
		-> std::vector<Reflection::GenericValue>
		{
			std::string_view requested = inputs.at(0).AsStringView();
			const auto& it = s_ComponentNameToType.find(requested);

			if (it == s_ComponentNameToType.end())
				RAISE_RT("Invalid component");

			GameObject* obj = static_cast<GameObject*>(p);
			obj->RemoveComponent(it->second);

			return {};
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
		RAISE_RTF(
			"Tried to GameObject::FromGenericValue, but GenericValue had Type '{}' instead",
			Reflection::TypeAsString(gv.Type)
		);
		
	return GameObject::GetObjectById(static_cast<uint32_t>(gv.Val.Int));
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
	if (Id == PHX_GAMEOBJECT_NULL_ID || Id == 0 || Id >= s_WorldArray.size())
		return nullptr;

	GameObject& obj = s_WorldArray[Id];
	
	return obj.Valid ? &obj : nullptr;
}

void GameObject::IncrementHardRefs()
{
	m_HardRefCount++;

	if (m_HardRefCount > UINT16_MAX - 1)
		RAISE_RT("Too many hard refs!");
}

void GameObject::DecrementHardRefs()
{
	if (m_HardRefCount == 0)
		RAISE_RT("Tried to decrement hard refs, with no hard references!");

	m_HardRefCount--;

	if (m_HardRefCount == 0)
		Destroy();
}

void GameObject::Destroy()
{
	ZoneScoped;

	assert(Valid);

	if (!IsDestructionPending)
	{
		this->SetParent(nullptr);
		this->IsDestructionPending = true;

		DecrementHardRefs(); // removes the reference in `::Create`
	}

	if (m_HardRefCount == 0 && Valid)
	{
		for (const ReflectorRef& ref : Components)
			s_ComponentManagers[(size_t)ref.Type]->DeleteComponent(ref.Id);

		Components.clear();

		for (GameObject* child : this->GetChildren())
			child->Destroy();

		Valid = false;
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

static uint32_t NullGameObjectIdValue = PHX_GAMEOBJECT_NULL_ID;

bool GameObject::IsDescendantOf(const GameObject* Object) const
{
	GameObject* current = this->GetParent();
	while (current)
	{
		if (current == Object)
			return true;
		current = current->GetParent();
	}

	return false;
}

void GameObject::SetParent(GameObject* newParent)
{
	ZoneScoped;

	if (this->IsDestructionPending)
		RAISE_RTF(
			"Tried to re-parent '{}' (ID:{}) to '{}' (ID:{}), but it's Parent has been locked due to `::Destroy`",

			GetFullName(),
			this->ObjectId,
			newParent ? newParent->GetFullName() : "<NULL>",
			newParent ? newParent->ObjectId : NullGameObjectIdValue
		);
	
	if (newParent != this)
	{
		if (newParent && newParent->IsDescendantOf(this))
			RAISE_RTF(
				"Tried to make object ID:{} ('{}') a descendant of itself",
				this->ObjectId, GetFullName()
			);
	}
	else
		RAISE_RTF(
			"Tried to make object ID:{} ('{}') it's own parent",
			this->ObjectId, GetFullName()
		);

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
	{
		this->ForEachDescendant([](GameObject* d) -> bool {
			d->OwningWorkspace = PHX_GAMEOBJECT_NULL_ID;
			return true;
		});
		this->OwningWorkspace = PHX_GAMEOBJECT_NULL_ID;

		return;
	}

	uint32_t newOwningWorkspace = newParent->OwningWorkspace;

	if (newParent->FindComponent<EcWorkspace>())
		newOwningWorkspace = newParent->ObjectId;

	if (newOwningWorkspace != this->OwningWorkspace)
	{
		this->ForEachDescendant([newOwningWorkspace](GameObject* d) -> bool {
			d->OwningWorkspace = newOwningWorkspace;
			return true;
		});
		this->OwningWorkspace = newOwningWorkspace;

		if (EcTransform* ct = this->FindComponent<EcTransform>())
			ct->RecomputeTransformTree();
	}

	this->Parent = newParent->ObjectId;
	newParent->AddChild(this);
}

void GameObject::AddChild(GameObject* c)
{
	ZoneScoped;

	if (c->ObjectId == this->ObjectId)
		RAISE_RTF(
			"::AddChild called on Object ID:{} (`{}`) with itself as the adopt'ed",
			this->ObjectId, GetFullName()
		);

	auto it = std::find(Children.begin(), Children.end(), c->ObjectId);

	if (it == Children.end())
	{
		Children.push_back(c->ObjectId);
		//c->IncrementHardRefs();

		if (EcTransform* ct = c->FindComponent<EcTransform>(); ct && this->FindComponent<EcTransform>())
			ct->RecomputeTransformTree(); // the child is be affected by this object's transform
	}
}

void GameObject::RemoveChild(uint32_t id)
{
	auto it = std::find(Children.begin(), Children.end(), id);

	if (it != Children.end())
	{
		Children.erase(it);

		// TODO HACK check ::Destroy
		//if (GameObject* ch = GameObject::GetObjectById(id))
		//	ch->DecrementHardRefs();
	}
	else
		RAISE_RTF("ID:{} is _not my ({}) sonnn~_", ObjectId, id);
}

Reflection::GenericValue GameObject::s_ToGenericValue(const GameObject* Object)
{
	Reflection::GenericValue gv;
	gv.Type = Reflection::ValueType::GameObject;
	gv.Val.Int = Object ? Object->ObjectId : PHX_GAMEOBJECT_NULL_ID;
	
	return gv;
}

Reflection::GenericValue GameObject::ToGenericValue() const
{
	return s_ToGenericValue(this);
}

GameObject* GameObject::GetParent() const
{
	return GameObject::GetObjectById(this->Parent);
}

void GameObject::ForEachChild(const std::function<bool(GameObject*)>& Callback)
{
	for (uint32_t id : Children)
	{
		GameObject* child = GameObject::GetObjectById(id);
		assert(child);
		
		if (bool shouldContinue = Callback(child); !shouldContinue)
			break;
	}
}

bool GameObject::ForEachDescendant(const std::function<bool(GameObject*)>& Callback)
{
	for (uint32_t id : Children)
	{
		GameObject* child = GameObject::GetObjectById(id);
		assert(child);

		if (bool shouldContinue = Callback(child); !shouldContinue)
			return true;

		if (child->ForEachDescendant(Callback))
			return true;
	}

	return false;
}

std::vector<GameObject*> GameObject::GetChildren()
{
	std::vector<GameObject*> children;
	children.reserve(Children.size());

	for (uint32_t id : Children)
	{
		GameObject* child = GameObject::GetObjectById(id);
		assert(child);
		children.push_back(child);
	}

	return children;
}

std::vector<GameObject*> GameObject::GetDescendants()
{
	ZoneScoped;

	std::vector<GameObject*> descendants;
	descendants.reserve(Children.size());

	for (uint32_t id : Children)
	{
		GameObject* child = GameObject::GetObjectById(id);
		assert(child);
		descendants.push_back(child);

		std::vector<GameObject*> childrenDescendants = child->GetDescendants();
		std::copy(childrenDescendants.begin(), childrenDescendants.end(), std::back_inserter(descendants));
	}

	return descendants;
}

GameObject* GameObject::FindChild(const std::string_view& ChildName)
{
	for (uint32_t index = 0; index < Children.size(); index++)
	{
		GameObject* child = GameObject::GetObjectById(Children[index]);
		assert(child);

		if (child->Name == ChildName)
			return child;
	}

	return nullptr;
}


GameObject* GameObject::FindChildWithComponent(EntityComponent Component)
{
	for (uint32_t index = 0; index < Children.size(); index++)
	{
		GameObject* child = GameObject::GetObjectById(Children[index]);
		assert(child);

		if (child->FindComponentByType(Component))
			return child;
	}

	return nullptr;
}

uint32_t GameObject::AddComponent(EntityComponent Type)
{
	PHX_ENSURE(Valid);

	if (FindComponentByType(Type))
		RAISE_RT("Already have that component");
	
	IComponentManager* manager = GameObject::s_ComponentManagers[(size_t)Type];
	Components.emplace_back(manager->CreateComponent(this), Type);

	uint32_t componentId = Components.back().Id;

	for (const auto& it : manager->GetProperties())
	{
		ComponentApis.Properties[it.first] = &it.second;
		MemberToComponentMap[it.first] = Components.back();
	}
	for (const auto& it : manager->GetMethods())
	{
		ComponentApis.Methods[it.first] = &it.second;
		MemberToComponentMap[it.first] = Components.back();
	}
	for (const auto& it : manager->GetEvents())
	{
		ComponentApis.Events[it.first] = &it.second;
		MemberToComponentMap[it.first] = Components.back();
	}

	return componentId;
}

void GameObject::RemoveComponent(EntityComponent Type)
{
	for (auto it = Components.begin(); it < Components.end(); it++)
		if (it->Type == Type)
		{
			Components.erase(it);

			IComponentManager* manager = GameObject::s_ComponentManagers[(size_t)Type];
			manager->DeleteComponent(it->Id);

			for (const auto& it2 : manager->GetProperties())
			{
				ComponentApis.Properties.erase(it2.first);
				MemberToComponentMap.erase(it2.first);
			}
			for (const auto& it2 : manager->GetMethods())
			{
				ComponentApis.Methods.erase(it2.first);
				MemberToComponentMap.erase(it2.first);
			}
			for (const auto& it2 : manager->GetEvents())
			{
				ComponentApis.Events.erase(it2.first);
				MemberToComponentMap.erase(it2.first);
			}

			return;
		}
	
	RAISE_RT("Don't have that component");
}

const Reflection::PropertyDescriptor* GameObject::FindProperty(const std::string_view& PropName, ReflectorRef* FromComponent)
{
	ReflectorRef dummyFc;
	FromComponent = FromComponent ? FromComponent : &dummyFc;

	if (auto it = s_Api.Properties.find(PropName); it != s_Api.Properties.end())
	{
		FromComponent->Id = ObjectId;
		return &it->second;
	}

	if (auto it = ComponentApis.Properties.find(PropName); it != ComponentApis.Properties.end())
	{
		*FromComponent = MemberToComponentMap[PropName];
		return it->second;
	}

	return nullptr;
}
const Reflection::MethodDescriptor* GameObject::FindMethod(const std::string_view& FuncName, ReflectorRef* FromComponent)
{
	ReflectorRef dummyFc;
	FromComponent = FromComponent ? FromComponent : &dummyFc;

	if (auto it = s_Api.Methods.find(FuncName); it != s_Api.Methods.end())
	{
		FromComponent->Id = ObjectId;
		return &it->second;
	}

	if (auto it = ComponentApis.Methods.find(FuncName); it != ComponentApis.Methods.end())
	{
		*FromComponent = MemberToComponentMap[FuncName];
		return it->second;
	}

	return nullptr;
}
const Reflection::EventDescriptor* GameObject::FindEvent(const std::string_view& EventName, ReflectorRef* Handle)
{
	ReflectorRef dummyHandle;
	Handle = Handle ? Handle : &dummyHandle;

	if (auto it = s_Api.Events.find(EventName); it != s_Api.Events.end())
	{
		Handle->Id = ObjectId;
		return &it->second;
	}

	if (auto it = ComponentApis.Events.find(EventName); it != ComponentApis.Events.end())
	{
		*Handle = MemberToComponentMap[EventName];
		return it->second;
	}

	return nullptr;
}

Reflection::GenericValue GameObject::GetPropertyValue(const std::string_view& PropName)
{
	ReflectorRef ref;

	if (const Reflection::PropertyDescriptor* prop = FindProperty(PropName, &ref))
	{
		Reflection::GenericValue gv = prop->Get(ref.Referred());
		assert(Reflection::TypeFits(prop->Type, gv.Type));

		return prop->Get(ref.Referred());
	}

	RAISE_RTF("Invalid property '{}' in GetPropertyValue", PropName);
}
void GameObject::SetPropertyValue(const std::string_view& PropName, const Reflection::GenericValue& Value)
{
	ReflectorRef ref;

	if (const Reflection::PropertyDescriptor* prop = FindProperty(PropName, &ref))
	{
		prop->Set(ref.Referred(), Value);
		
		return;
	}

	RAISE_RTF("Invalid property '{}' in SetPropertyValue", PropName);
}

std::vector<Reflection::GenericValue> GameObject::CallMethod(const std::string_view& FuncName, const std::vector<Reflection::GenericValue>& Inputs)
{
	ReflectorRef ref;

	if (const Reflection::MethodDescriptor* func = FindMethod(FuncName, &ref))
		return func->Func(ref.Referred(), Inputs);

	RAISE_RTF("Invalid function '{}' in CallMethod", FuncName);
}

Reflection::PropertyMap GameObject::GetProperties() const
{
	// base APIs always take priority for consistency
	Reflection::PropertyMap cumulativeProps = ComponentApis.Properties;

	for (const auto& it : s_Api.Properties)
		cumulativeProps.insert(std::pair(it.first, &it.second));

	return cumulativeProps;
}
Reflection::MethodMap GameObject::GetMethods() const
{
	Reflection::MethodMap cumulativeFuncs = ComponentApis.Methods;

	for (const auto& it : s_Api.Methods)
		cumulativeFuncs.insert(std::pair(it.first, &it.second));

	return cumulativeFuncs;
}
Reflection::EventMap GameObject::GetEvents() const
{
	Reflection::EventMap cumulativeEvents = ComponentApis.Events;

	for (const auto& it : s_Api.Events)
		cumulativeEvents.insert(std::pair(it.first, &it.second));

	return cumulativeEvents;
}

void* GameObject::FindComponentByType(EntityComponent Type)
{
	for (const ReflectorRef& ref : Components)
		if (ref.Type == Type)
			return GameObject::s_ComponentManagers[(size_t)Type]->GetComponent(ref.Id);
	
	return nullptr;
}

GameObject* GameObject::Create()
{
	if (s_WorldArray.size() == 0)
	{
		s_WorldArray.emplace_back();
		s_WorldArray[0].Name = "<RESERVED INVALID SLOT>";
	}

	uint32_t numObjects = static_cast<uint32_t>(s_WorldArray.size());

	if (numObjects >= UINT32_MAX - 1)
		RAISE_RT("Reached end of GameObject ID space (2^32 - 1)");

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
		RAISE_RTF("Invalid Component Name '{}'", FirstComponent);
	else
		return GameObject::Create(it->second);
}

static void dumpProperties(const Reflection::StaticPropertyMap& Properties, nlohmann::json& Json)
{
	for (const auto& propIt : Properties)
	{
		std::string pstr{ Reflection::TypeAsString(propIt.second.Type) };

		if (!propIt.second.Set)
			pstr += " READONLY";
		
		else if (!propIt.second.Serializes)
			pstr += " RUNTIMEONLY";

		Json["Properties"][propIt.first] = pstr;
	}
}

static void dumpMethods(const Reflection::StaticMethodMap& Functions, nlohmann::json& Json)
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

		Json["Methods"][funcIt.first] = std::format("({}) -> ({})", istring, ostring);
	}
}

static void dumpEvents(const Reflection::StaticEventMap& Events, nlohmann::json& Json)
{
	for (const auto& eventIt : Events)
	{
		std::string argstring = "";

		for (Reflection::ValueType a : eventIt.second.CallbackInputs)
			argstring += std::string(Reflection::TypeAsString(a)) + ", ";

		argstring = argstring.substr(0, argstring.size() - 2);

		Json["Events"][eventIt.first] = std::format("({})", argstring);
	}
}

nlohmann::json GameObject::DumpApiToJson()
{
	nlohmann::json dump;
	
	nlohmann::json& gameObjectApi = dump["Base"];
	nlohmann::json& componentApi = dump["Components"];

	s_AddObjectApi(); // make sure we have the api
	dumpProperties(s_Api.Properties, gameObjectApi);
	dumpMethods(s_Api.Methods, gameObjectApi);
	dumpEvents(s_Api.Events, gameObjectApi);
	
	for (size_t i = 0; i < (size_t)EntityComponent::__count; i++)
	{
		IComponentManager* manager = s_ComponentManagers[i];

		if (!manager)
			continue;

		nlohmann::json& api = componentApi[s_EntityComponentNames[i]];
		api = nlohmann::json::object();

		dumpProperties(manager->GetProperties(), api);
		dumpMethods(manager->GetMethods(), api);
		dumpEvents(manager->GetEvents(), api);
	}

	return dump;
}
