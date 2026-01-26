#include <tracy/Tracy.hpp>

#include "datatype/GameObject.hpp"
#include "component/Transform.hpp"
#include "component/DataModel.hpp"
#include "component/Workspace.hpp"
#include "History.hpp"
#include "Log.hpp"

const std::unordered_map<std::string_view, EntityComponent> s_ComponentNameToType = []()
{
	std::unordered_map<std::string_view, EntityComponent> map;
	map.reserve((size_t)EntityComponent::__count);

	for (size_t i = 0; i < (size_t)EntityComponent::__count; i++)
		map[s_EntityComponentNames[i]] = (EntityComponent)i;

	return map;
}();

const Reflection::StaticApi GameObject::s_Api = Reflection::StaticApi{
	.Properties = {
		REFLECTION_PROPERTY_SIMPLE(GameObject, Name, String),
		REFLECTION_PROPERTY_SIMPLE(GameObject, Enabled, Boolean),
		REFLECTION_PROPERTY_SIMPLE(GameObject, Serializes, Boolean),
		REFLECTION_PROPERTY("ObjectId", Integer, REFLECTION_PROPERTY_GET_SIMPLE(GameObject, ObjectId), nullptr),

		REFLECTION_PROPERTY(
			"Exists",
			Boolean,
			[](void*) -> Reflection::GenericValue
			{
				return true; // actual logic is handled in `obj_index`
			},
			nullptr
		),

		{ "Parent", Reflection::PropertyDescriptor(
			"Parent",
			REFLECTION_OPTIONAL(GameObject),
			[](void* p) -> Reflection::GenericValue
			{
				return static_cast<GameObject*>(p)->GetParent()->ToGenericValue();
			},
			[](void* p, const Reflection::GenericValue& gv)
			{
				static_cast<GameObject*>(p)->SetParent(GameObject::FromGenericValue(gv));
			}
		) }
	},

	.Methods = {
		{ "Destroy", Reflection::MethodDescriptor{
			{},
			{},
			[](void* p, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
			{
				static_cast<GameObject*>(p)->Destroy();
				return {};
			}
		} },

		{ "GetFullName", Reflection::MethodDescriptor{
			{},
			{ Reflection::ValueType::String },
			[](void* p, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
			{
				return { static_cast<GameObject*>(p)->GetFullName() };
			}
		} },

		{ "ForEachChild", Reflection::MethodDescriptor{
			{ Reflection::ValueType::Function },
			{},
			[](void* p, const std::vector<Reflection::GenericValue>& gv) -> std::vector<Reflection::GenericValue>
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
		} },

		{ "ForEachDescendant", Reflection::MethodDescriptor{
			{ Reflection::ValueType::Function },
			{},
			[](void* p, const std::vector<Reflection::GenericValue>& gv) -> std::vector<Reflection::GenericValue>
			{
				const Reflection::GenericFunction& gf = gv.at(0).AsFunction(); // damn

				static_cast<GameObject*>(p)->ForEachDescendant(
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
		} },

		{ "GetChildren", Reflection::MethodDescriptor{
			{},
			{ Reflection::ValueType::Array },
			[](void* p, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
			{
				std::vector<Reflection::GenericValue> retval;
				for (GameObject* g : static_cast<GameObject*>(p)->GetChildren())
					retval.push_back(g->ToGenericValue());

				// ctor for ValueType::Array
				return { Reflection::GenericValue(retval) };
			}
		} },

		{ "GetDescendants", Reflection::MethodDescriptor{
			{},
			{ Reflection::ValueType::Array },
			[](void* p, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
			{
				std::vector<Reflection::GenericValue> retval;
				for (GameObject* g : static_cast<GameObject*>(p)->GetDescendants())
					retval.push_back(g->ToGenericValue());

				// ctor for ValueType::Array
				return { Reflection::GenericValue(retval) };
			}
		} },

		{ "Duplicate", Reflection::MethodDescriptor{
			{},
			{ Reflection::ValueType::GameObject },
			[](void* p, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
			{
				GameObject* g = static_cast<GameObject*>(p);
				return { g->Duplicate()->ToGenericValue() };
			}
		} },

		{ "MergeWith", Reflection::MethodDescriptor{
			{ Reflection::ValueType::GameObject },
			{},
			[](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
			{
				GameObject* g = static_cast<GameObject*>(p);
				GameObject* o = GameObject::FromGenericValue(inputs[0]);

				g->MergeWith(o);
				return { g->ToGenericValue() };
			}
		} },

		{ "FindChild", Reflection::MethodDescriptor{
			{ Reflection::ValueType::String },
			{ REFLECTION_OPTIONAL(GameObject) },
			[](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
			{
				GameObject* g = static_cast<GameObject*>(p);
				return { g->FindChild(inputs[0].AsStringView())->ToGenericValue() };
			}
		} },

		{ "FindChildWithComponent", Reflection::MethodDescriptor{
			{ Reflection::ValueType::String },
			{ REFLECTION_OPTIONAL(GameObject) },
			[](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
			{
				EntityComponent ec = GameObject::s_FindComponentTypeByName(inputs[0].AsStringView());
				if (ec == EntityComponent::None)
					RAISE_RTF("Invalid component type '{}'", inputs[0].AsStringView());

				GameObject* g = static_cast<GameObject*>(p);
				return { g->FindChildWithComponent(ec)->ToGenericValue() };
			}
		} },

		{ "GetComponents", Reflection::MethodDescriptor{
			{},
			{ Reflection::ValueType::Array },
			[](void* p, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
			{
				std::vector<Reflection::GenericValue> ret;

				for (const ReflectorRef& ref : static_cast<GameObject*>(p)->Components)
					ret.emplace_back(s_EntityComponentNames[(size_t)ref.Type]);

				return { ret };
			}
		} },

		{ "HasComponent", Reflection::MethodDescriptor{
			{ Reflection::ValueType::String },
			{ Reflection::ValueType::Boolean },
			[](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
			{
				EntityComponent ec = GameObject::s_FindComponentTypeByName(inputs[0].AsStringView());
				if (ec == EntityComponent::None)
					RAISE_RT("Invalid component");

				for (const ReflectorRef& ref : static_cast<GameObject*>(p)->Components)
					if (ref.Type == ec)
						return { true };

				return { false };
			}
		} },

		{ "AddComponent", Reflection::MethodDescriptor{
			{ Reflection::ValueType::String },
			{},
			[](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
			{
				EntityComponent ec = GameObject::s_FindComponentTypeByName(inputs[0].AsStringView());
				if (ec == EntityComponent::None)
					RAISE_RT("Invalid component");

				GameObject* obj = static_cast<GameObject*>(p);
				obj->AddComponent(ec);

				return {};
			}
		} },

		{ "RemoveComponent", Reflection::MethodDescriptor{
			{ Reflection::ValueType::String },
			{},
			[](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
			{
				EntityComponent ec = GameObject::s_FindComponentTypeByName(inputs[0].AsStringView());
				if (ec == EntityComponent::None)
					RAISE_RT("Invalid component");

				GameObject* obj = static_cast<GameObject*>(p);
				obj->RemoveComponent(ec);

				return {};
			}
		} },
	}
};

// https://stackoverflow.com/a/75891310
struct HandleHasher
{
	size_t operator()(const ObjectHandle& a) const
    {
        return a.Reference.TargetId;
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
	ObjectHandle Root,
	// u_m < og-child, vector < pair < clone-object-referencing-ogchild, property-referencing-ogchild > > >
	std::unordered_map<ObjectHandle, std::vector<std::pair<ObjectHandle, std::string_view>>, HandleHasher> OverwritesMap = {},
	std::unordered_map<ObjectHandle, ObjectHandle, HandleHasher> OriginalToCloneMap = {}
)
{
	ObjectHandle newObj = GameObject::Create();

	for (const ReflectorRef& ref : Root->Components)
		newObj->AddComponent(ref.Type);

	auto overwritesIt = OverwritesMap.find(Root);

	if (overwritesIt != OverwritesMap.end())
	{
		for (const std::pair<ObjectHandle, std::string_view>& overwrite : overwritesIt->second)
			// change the reference to the OG object to it's clone
			overwrite.first->SetPropertyValue(overwrite.second, newObj->ToGenericValue());

		overwritesIt->second.clear();
	}

	std::vector<GameObject*> rootDescsRaw = Root->GetDescendants();
	std::vector<ObjectHandle> rootDescs;
	rootDescs.reserve(rootDescsRaw.size());

	for (GameObject* d : rootDescsRaw)
		rootDescs.emplace_back(d);

	for (auto& it : Root->GetProperties())
	{
		if (!it.second->Set)
			continue; // read-only

		Reflection::GenericValue rootVal = Root->GetPropertyValue(it.first);

		if (rootVal.Type == Reflection::ValueType::GameObject)
		{
			ObjectHandle ref = GameObject::FromGenericValue(rootVal);
			if (!ref.HasValue())
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

	std::vector<ObjectHandle> chrefs;
	for (GameObject* ch : Root->GetChildren())
		chrefs.emplace_back(ch);

	for (const ObjectHandle& ch : chrefs)
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

EntityComponent GameObject::s_FindComponentTypeByName(const std::string_view& Name)
{
	const auto& it = s_ComponentNameToType.find(Name);

	if (it == s_ComponentNameToType.end())
		return EntityComponent::None;
	else
		return it->second;
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
		fullName = parent->Name + "." + fullName;
		curObject = parent;
	}

	return fullName;
}

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
			"Tried to re-parent {} to {}, but its Parent has been locked due to `:Destroy`",

			GetFullName(),
			newParent ? newParent->GetFullName() : "nil"
		);
	
	if (newParent != this)
	{
		if (newParent && newParent->IsDescendantOf(this))
			RAISE_RTF(
				"Tried to make {} a descendant of itself",
				GetFullName()
			);
	}
	else
		RAISE_RTF(
			"Tried to make {} its own parent",
			GetFullName()
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

	if (History* history = History::Get(); history->IsRecordingEnabled)
	{
		if (history->TargetDataModel == OwningDataModel || (newParent && (history->TargetDataModel == newParent->ObjectId || history->TargetDataModel == newParent->OwningDataModel)))
		{
			history->RecordEvent({
				.Target = { .Id = ObjectId, .Type = EntityComponent::None },
				.TargetObject = this,
				.Property = &s_Api.Properties.at("Parent"),
				.PreviousValue = GameObject::s_ToGenericValue(oldParent),
				.NewValue = GameObject::s_ToGenericValue(newParent)
			});
		}
	}

	if (!newParent)
	{
		this->ForEachDescendant([](GameObject* d) -> bool {
			d->OwningDataModel = PHX_GAMEOBJECT_NULL_ID;
			d->OwningWorkspace = PHX_GAMEOBJECT_NULL_ID;
			return true;
		});
		this->OwningDataModel = PHX_GAMEOBJECT_NULL_ID;
		this->OwningWorkspace = PHX_GAMEOBJECT_NULL_ID;

		return;
	}

	uint32_t newOwningDataModel = newParent->OwningDataModel;
	uint32_t newOwningWorkspace = newParent->OwningWorkspace;

	if (newParent->FindComponent<EcDataModel>())
		newOwningDataModel = newParent->ObjectId;

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

	if (newOwningDataModel != this->OwningDataModel)
	{
		this->ForEachDescendant([newOwningDataModel](GameObject* d) -> bool {
			d->OwningDataModel = newOwningDataModel;
			return true;
		});
		this->OwningDataModel = newOwningDataModel;

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
	// TODO GCC bug?? will segfault on the ternary unless this is marked `volatile`
	volatile Reflection::GenericValue gv;
	gv.Type = Reflection::ValueType::GameObject;
	gv.Val.Int = Object ? Object->ObjectId : PHX_GAMEOBJECT_NULL_ID;

	Reflection::GenericValue gv2;
	gv2.Type = Reflection::ValueType::GameObject;
	gv2.Val.Int = gv.Val.Int;

	return gv2;
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
	size_t beforeSize = s_WorldArray.size();

	for (uint32_t id : Children)
	{
		GameObject* child = GameObject::GetObjectById(id);
		assert(child);

		if (bool shouldContinue = Callback(child); !shouldContinue)
			break;

		if (s_WorldArray.size() != beforeSize)
			RAISE_RT("Callback cannot create objects"); // `this` may get re-allocated, very very bad
	}
}

bool GameObject::ForEachDescendant(const std::function<bool(GameObject*)>& Callback)
{
	size_t beforeSize = s_WorldArray.size();

	for (uint32_t id : Children)
	{
		GameObject* child = GameObject::GetObjectById(id);
		assert(child);

		if (bool shouldContinue = Callback(child); !shouldContinue)
			return true;

		if (s_WorldArray.size() != beforeSize)
			RAISE_RT("Callback cannot create objects"); // `this` may get re-allocated, very very bad

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

			Components.erase(it);

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
	ZoneScoped;
	ZoneText(PropName.data(), PropName.size());

	ReflectorRef ref;

	if (const Reflection::PropertyDescriptor* prop = FindProperty(PropName, &ref))
	{
		if (History* history = History::Get(); history->IsRecordingEnabled
			&& (OwningDataModel == history->TargetDataModel || ObjectId == history->TargetDataModel)
		)
		{
			Reflection::GenericValue prevValue = prop->Get(ref.Referred());
			prop->Set(ref.Referred(), Value);

			if (Value != prevValue)
			{
				history->RecordEvent({
					.Target = ref,
					.TargetObject = this,
					.Property = prop,
					.PreviousValue = prevValue,
					.NewValue = Value
				});
			}
		}
		else
		{
			prop->Set(ref.Referred(), Value);
		}

		return;
	}

	RAISE_RTF("Invalid property '{}' in SetPropertyValue", PropName);
}

Reflection::GenericValue GameObject::GetDefaultPropertyValue(const std::string_view& PropName)
{
	ZoneScoped;
	ZoneText(PropName.data(), PropName.size());

	ReflectorRef ref;
	if (const Reflection::PropertyDescriptor* prop = FindProperty(PropName, &ref))
	{
		if (ref.Type == EntityComponent::None)
		{
			static GameObject Defaults;
			return prop->Get((void*)&Defaults);
		}
		else
		{
			return s_ComponentManagers[(size_t)ref.Type]->GetDefaultPropertyValue(PropName);
		}
	}

	RAISE_RTF("Invalid property '{}' in GetDefaultPropertyValue", PropName);
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
		s_WorldArray[0].Valid = false;
	}

	uint32_t numObjects = static_cast<uint32_t>(s_WorldArray.size());

	if (numObjects >= UINT32_MAX - 1)
		RAISE_RT("Reached end of GameObject ID space (2^32 - 1)");

#ifndef NDEBUG

	// cause as many re-allocations as possible to catch stale pointers
	s_WorldArray.shrink_to_fit();

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
