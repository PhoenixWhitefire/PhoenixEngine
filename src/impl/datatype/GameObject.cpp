#include <tracy/Tracy.hpp>

#include "datatype/GameObject.hpp"
#include "Reflection.hpp"
#include "component/Transform.hpp"
#include "component/DataModel.hpp"
#include "component/Workspace.hpp"
#include "component/Sound.hpp"
#include "History.hpp"
#include "Log.hpp"

const Reflection::StaticApi GameObject::s_Api = Reflection::StaticApi{
    .Properties = {
        REFLECTION_PROPERTY_SIMPLE(GameObject, Name, String),
        REFLECTION_PROPERTY_SIMPLE(GameObject, Serializes, Boolean),
        REFLECTION_PROPERTY("ObjectId", Integer, REFLECTION_PROPERTY_GET_SIMPLE(GameObject, ObjectId), nullptr),

        REFLECTION_PROPERTY(
            "Enabled",
            Boolean,
            [](void* p) -> Reflection::GenericValue
            {
                GameObject* g = static_cast<GameObject*>(p);
                return g->GetEnabled();
            },
            [](void* p, const Reflection::GenericValue& gv)
            {
                GameObject* g = static_cast<GameObject*>(p);
                g->SetEnabled(gv.AsBoolean());
            }
        ),

        REFLECTION_PROPERTY(
            "TreeEnabled",
            Boolean,
            REFLECTION_PROPERTY_GET_SIMPLE(GameObject, TreeEnabled),
            nullptr
        ),

        { "Parent", Reflection::PropertyDescriptor{
            .Name = "Parent",
            .Get = [](void* p) -> Reflection::GenericValue
            {
                GameObject* g = static_cast<GameObject*>(p);
                return GameObject::s_ToGenericValue(g->GetParent());
            },
            .Set = [](void* p, const Reflection::GenericValue& gv)
            {
                GameObject* g = static_cast<GameObject*>(p);
                g->SetParent(GameObjectManager::Get()->FromGenericValue(gv));
            },
            .Type = REFLECTION_OPTIONAL(GameObject),
        } },
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
            REFLECTION_SPAN({ Reflection::ValueType::String }),
            [](void* p, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                return { static_cast<GameObject*>(p)->GetFullName() };
            }
        } },

        { "GetChildren", Reflection::MethodDescriptor{
            {},
            REFLECTION_SPAN({ Reflection::ValueType::Array }),
            [](void* p, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                std::vector<Reflection::GenericValue> retval;
                for (const ObjectHandle& g : static_cast<GameObject*>(p)->GetChildren())
                    retval.push_back(g->ToGenericValue());

                // ctor for ValueType::Array
                return { Reflection::GenericValue(retval) };
            }
        } },

        { "GetDescendants", Reflection::MethodDescriptor{
            {},
            REFLECTION_SPAN({ Reflection::ValueType::Array }),
            [](void* p, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                std::vector<Reflection::GenericValue> retval;
                for (const ObjectHandle& g : static_cast<GameObject*>(p)->GetDescendants())
                    retval.push_back(g->ToGenericValue());

                // ctor for ValueType::Array
                return { Reflection::GenericValue(retval) };
            }
        } },

        { "IsDescendantOf", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::GameObject }),
            REFLECTION_SPAN({ Reflection::ValueType::Boolean }),
            [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                GameObjectManager* objectManager = GameObjectManager::Get();

                GameObject* g = static_cast<GameObject*>(p);
                return { g->IsDescendantOf(objectManager->FromGenericValue(inputs[0])) };
            }
        } },

        { "Duplicate", Reflection::MethodDescriptor{
            {},
            REFLECTION_SPAN({ Reflection::ValueType::GameObject }),
            [](void* p, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                GameObject* g = static_cast<GameObject*>(p);
                return { g->Duplicate()->ToGenericValue() };
            }
        } },

        { "FindChild", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String }),
            REFLECTION_SPAN({ REFLECTION_OPTIONAL(GameObject) }),
            [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                GameObject* g = static_cast<GameObject*>(p);
                return { GameObject::s_ToGenericValue(g->FindChild(inputs[0].AsStringView())) };
            }
        } },

        { "FindChildWithComponent", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String }),
            REFLECTION_SPAN({ REFLECTION_OPTIONAL(GameObject) }),
            [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                EntityComponent ec = FindComponentTypeByName(inputs[0].AsStringView());
                if (ec == EntityComponent::None)
                    RAISE_RT("Invalid component '{}'", inputs[0].AsStringView());

                GameObject* g = static_cast<GameObject*>(p);
                return { GameObject::s_ToGenericValue(g->FindChildWithComponent(ec)) };
            }
        } },

        { "GetComponents", Reflection::MethodDescriptor{
            {},
            REFLECTION_SPAN({ Reflection::ValueType::Array }),
            [](void* p, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                std::vector<Reflection::GenericValue> ret;

                for (const ReflectorRef& ref : static_cast<GameObject*>(p)->Components)
                    ret.emplace_back(s_EntityComponentNames[(size_t)ref.Type]);

                return { Reflection::GenericValue(ret) };
            }
        } },

        { "HasComponent", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String }),
            REFLECTION_SPAN({ Reflection::ValueType::Boolean }),
            [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                EntityComponent ec = FindComponentTypeByName(inputs[0].AsStringView());
                if (ec == EntityComponent::None)
                    RAISE_RT("Invalid component '{}'", inputs[0].AsStringView());

                for (const ReflectorRef& ref : static_cast<GameObject*>(p)->Components)
                    if (ref.Type == ec)
                        return { true };

                return { false };
            }
        } },

        { "AddComponent", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String }),
            {},
            [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                EntityComponent ec = FindComponentTypeByName(inputs[0].AsStringView());
                if (ec == EntityComponent::None)
                    RAISE_RT("Invalid component '{}'", inputs[0].AsStringView());

                GameObject* obj = static_cast<GameObject*>(p);
                obj->AddComponent(ec);

                return {};
            }
        } },

        { "RemoveComponent", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String }),
            {},
            [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                EntityComponent ec = FindComponentTypeByName(inputs[0].AsStringView());
                if (ec == EntityComponent::None)
                    RAISE_RT("Invalid component '{}'", inputs[0].AsStringView());

                GameObject* obj = static_cast<GameObject*>(p);
                obj->RemoveComponent(ec);

                return {};
            }
        } },

        { "AsComponent", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String }),
            REFLECTION_SPAN({ REFLECTION_OPTIONAL(GameObject) }),
            [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                EntityComponent ec = FindComponentTypeByName(inputs[0].AsStringView());
                if (ec == EntityComponent::None)
                    RAISE_RT("Invalid component '{}'", inputs[0].AsStringView());

                GameObject* obj = static_cast<GameObject*>(p);

                if (std::find_if(obj->Components.begin(), obj->Components.end(), [ec](const auto& a) { return a.Type == ec; }) == obj->Components.end())
                    return { Reflection::GenericValue::Null() };
                else
                    return { obj->ToGenericValue() };
            }
        } },

        { "AddTag", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String }),
            {},
            [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                GameObject* obj = static_cast<GameObject*>(p);
                obj->AddTag(inputs[0].AsString());

                return {};
            }
        } },
        { "RemoveTag", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String }),
            {},
            [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                GameObject* obj = static_cast<GameObject*>(p);
                obj->RemoveTag(inputs[0].AsString());

                return {};
            }
        } },
        { "HasTag", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String }),
            REFLECTION_SPAN({ Reflection::ValueType::Boolean }),
            [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                GameObject* obj = static_cast<GameObject*>(p);

                return { obj->HasTag(inputs[0].AsString()) };
            }
        } },
        { "GetTags", Reflection::MethodDescriptor{
            {},
            REFLECTION_SPAN({ Reflection::ValueType::Array }),
            [](void* p, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                GameObject* obj = static_cast<GameObject*>(p);
                std::vector<Reflection::GenericValue> tags;

                for (uint16_t tagId : obj->Tags)
                    tags.emplace_back(GameObjectManager::Get()->Collections[tagId].Name);

                return { Reflection::GenericValue(tags) };
            }
        } },
    },

    .Events = {
        REFLECTION_EVENT(GameObject, OnTagAdded, Reflection::ValueType::String),
        REFLECTION_EVENT(GameObject, OnTagRemoved, Reflection::ValueType::String),
        REFLECTION_EVENT(GameObject, OnTreeEnabledChanged, Reflection::ValueType::Boolean),
        REFLECTION_EVENT(GameObject, OnWorkspaceChanged, Reflection::ValueType::GameObject, Reflection::ValueType::GameObject),
    }
};

// https://stackoverflow.com/a/75891310
struct HandleHasher
{
    size_t operator()(const ObjectHandle& a) const noexcept
    {
        return a.Reference.TargetId;
    }
};

void* ReflectorRef::Referred() const
{
    if (Type == EntityComponent::None)
        return (void*)GameObjectManager::Get()->FindById(Id);
    else
        return GetComponentManagerByComponentType(Type)->GetComponent(Id);
}

static ObjectHandle cloneRecursive(
    const ObjectHandle& Root,
    // u_m < og-child, vector < pair < clone-object-referencing-ogchild, property-referencing-ogchild > > >
    std::unordered_map<ObjectHandle, std::vector<std::pair<ObjectHandle, std::string_view>>, HandleHasher> OverwritesMap = {},
    std::unordered_map<ObjectHandle, ObjectHandle, HandleHasher> OriginalToCloneMap = {}
)
{
    GameObjectManager* objectManager = GameObjectManager::Get();
    ObjectHandle newObj = objectManager->Create();

    for (const ReflectorRef& ref : Root->Components)
        newObj->AddComponent(ref.Type);

    for (uint16_t tag : Root->Tags)
        newObj->Tags.push_back(tag);

    auto overwritesIt = OverwritesMap.find(Root);

    if (overwritesIt != OverwritesMap.end())
    {
        for (const std::pair<ObjectHandle, std::string_view>& overwrite : overwritesIt->second)
            // change the reference to the OG object to its clone
            overwrite.first->SetPropertyValue(overwrite.second, newObj->ToGenericValue());

        overwritesIt->second.clear();
    }

    const std::vector<ObjectHandle> rootDescs = Root->GetDescendants();

    for (auto& it : Root->GetProperties())
    {
        if (!it.second->Set)
            continue; // read-only
        if (!it.second->Serializes)
            continue; // Transform.Transform

        Reflection::GenericValue rootVal = Root->GetPropertyValue(it.first);

        if (rootVal.Type == Reflection::ValueType::GameObject)
        {
            ObjectHandle ref = objectManager->FromGenericValue(rootVal);
            if (!ref.HasValue())
                continue;

            auto otcit = OriginalToCloneMap.find(ref);

            if (otcit != OriginalToCloneMap.end())
                newObj->SetPropertyValue(it.first, otcit->second->ToGenericValue());
            else
            {
                if (std::find(rootDescs.begin(), rootDescs.end(), ref) != rootDescs.end())
                    OverwritesMap[ref].push_back(std::pair(newObj, it.first));

                newObj->SetPropertyValue(it.first, rootVal);
            }
        }
        else
            newObj->SetPropertyValue(it.first, rootVal);
    }

    for (const ObjectHandle& ch : Root->GetChildren())
    {
        if (ch->Serializes)
            cloneRecursive(ch, OverwritesMap, OriginalToCloneMap)->SetParent(newObj);
    }

    return newObj;
}

ObjectHandle GameObject::Duplicate()
{
    ZoneScoped;
    return cloneRecursive(this);
}

GameObject* GameObjectManager::FromGenericValue(const Reflection::GenericValue& gv)
{
    if (gv.Type == Reflection::ValueType::Null)
        return nullptr;

    if (gv.Type != Reflection::ValueType::GameObject)
        RAISE_RT(
            "Tried to GameObject::FromGenericValue, but GenericValue had Type '{}' instead",
            Reflection::TypeAsString(gv.Type)
        );

    assert(!GameObjectManager::Get()->FindById(static_cast<uint32_t>(gv.Val.Int)) ? gv.Val.Int == PHX_GAMEOBJECT_NULL_ID : true);
    return GameObjectManager::Get()->FindById(static_cast<uint32_t>(gv.Val.Int));
}

GameObject* GameObjectManager::FindById(uint32_t Id)
{
    assert(Id > 0);

    if (Id == PHX_GAMEOBJECT_NULL_ID || Id >= WorldArray.size())
        return nullptr;

    GameObject& obj = WorldArray[Id];
    assert(obj.Valid);
    return &obj;
}

void GameObject::IncrementHardRefs()
{
    HardRefCount++;

    if (HardRefCount > UINT16_MAX - 1)
        RAISE_RT("Too many hard refs!");
}

void GameObject::DecrementHardRefs()
{
    if (HardRefCount == 0)
        RAISE_RT("Tried to decrement hard refs, with no hard references!");

    HardRefCount--;

    if (HardRefCount == 0)
        Destroy();
}

void GameObject::Destroy()
{
    ZoneScoped;
    assert(Valid);

    // Editor: Operation needs to be undo-able
    if (History* history = History::Get(); history->IsRecordingEnabled
        && (OwningDataModel == history->TargetDataModel || ObjectId == history->TargetDataModel)
    )
    {
        for (const ObjectHandle& child : this->GetChildren())
            child->Destroy();

        SetParent(nullptr); // children OwningDataModel should not change until they all detach
        return;
    }

    if (!IsDestructionPending)
    {
        GameObjectManager* ObjectManager = GameObjectManager::Get();
        HardRefCount++;

        for (uint16_t tagId : Tags)
        {
            GameObjectManager::Collection& collection = ObjectManager->Collections[tagId];
            if (const auto& it = std::find(collection.Items.begin(), collection.Items.end(), ObjectId); it != collection.Items.end())
                collection.Items.erase(it);

            Reflection::SignalEvent(collection.RemovedEvent.Callbacks, { this->ToGenericValue() }, "TagRemovedSignal");
            Reflection::SignalEvent(OnTagRemovedCallbacks, { ObjectManager->Collections[tagId].Name }, "GameObject.OnTagRemoved");
        }
        Tags.clear();

        for (const ReflectorRef& ref : Components)
        {
            if (ref.Type == EntityComponent::DataModel)
            {
                ((EcDataModel*)ref.Referred())->Close();
                break;
            }
        }

        while (!Components.empty())
            RemoveComponent(Components.back().Type);

        for (auto& [ _, event ] : s_Api.Events)
            event.Cleanup((void*)this);

        this->SetParent(nullptr);

        assert(HardRefCount > 0);
        HardRefCount--;
        this->IsDestructionPending = true;
    }

    if (HardRefCount == 0 && Valid)
    {
        for (const ObjectHandle& child : this->GetChildren())
            child->Destroy();

        GameObjectManager* objectManager = GameObjectManager::Get();
        NextFreeId = objectManager->NextFreeId;
        objectManager->NextFreeId = ObjectId; // stole this stick from `lua_ref`

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

void GameObject::EvaluateOwners()
{
    ZoneScoped;

    GameObject* parent = GetParent();

    if (parent)
    {
        uint32_t newOwningDataModel = parent->OwningDataModel;
        uint32_t newOwningWorkspace = parent->OwningWorkspace;

        if (this->FindComponent<EcDataModel>())
            newOwningDataModel = this->ObjectId;
        else if (parent->FindComponent<EcDataModel>())
            newOwningDataModel = parent->ObjectId;

        if (this->FindComponent<EcWorkspace>())
            newOwningWorkspace = this->ObjectId;
        else if (parent->FindComponent<EcWorkspace>())
            newOwningWorkspace = parent->ObjectId;

        if (newOwningWorkspace != this->OwningWorkspace)
        {
            GameObjectManager* objectManager = GameObjectManager::Get();
            Reflection::SignalEvent(
                OnWorkspaceChangedCallbacks,
                {
                    GameObject::s_ToGenericValue(objectManager->FindById(OwningWorkspace)),
                    GameObject::s_ToGenericValue(objectManager->FindById(newOwningWorkspace)),
                },
                "GameObject.OnWorkspaceChanged"
            );

            this->ForEachDescendant([newOwningWorkspace](const ObjectHandle& d) noexcept -> bool {
                d->OwningWorkspace = newOwningWorkspace;
                return true;
            });
            this->OwningWorkspace = newOwningWorkspace;
        }

        if (newOwningDataModel != this->OwningDataModel)
        {
            this->ForEachDescendant([newOwningDataModel](const ObjectHandle& d) noexcept -> bool {
                d->OwningDataModel = newOwningDataModel;
                return true;
            });
            this->OwningDataModel = newOwningDataModel;
        }
    }
    else
    {
        uint32_t newOwningDataModel = UINT32_MAX;
        uint32_t newOwningWorkspace = UINT32_MAX;

        if (this->FindComponent<EcDataModel>())
            newOwningDataModel = ObjectId;

        if (this->FindComponent<EcWorkspace>())
            newOwningWorkspace = ObjectId;

        GameObjectManager* objectManager = GameObjectManager::Get();
        Reflection::SignalEvent(
            OnWorkspaceChangedCallbacks,
            {
                GameObject::s_ToGenericValue(objectManager->FindById(OwningWorkspace)),
                GameObject::s_ToGenericValue(objectManager->FindById(newOwningWorkspace)),
            },
            "GameObject.OnWorkspaceChanged"
        );

        this->ForEachDescendant([newOwningDataModel, newOwningWorkspace](const ObjectHandle& d) noexcept -> bool {
            d->OwningDataModel = newOwningDataModel;
            d->OwningWorkspace = newOwningWorkspace;
            return true;
        });
        this->OwningDataModel = newOwningDataModel;
        this->OwningWorkspace = newOwningWorkspace;
    }

    if (EcTransform* ct = this->FindComponent<EcTransform>())
        ct->RecomputeTransformTree();
}

void GameObject::SetParent(const ObjectHandle& newParent)
{
    ZoneScoped;

    if (this->IsDestructionPending)
        RAISE_RT(
            "Tried to re-parent {} to {}, but its Parent has been locked due to `:Destroy`",
            GetFullName(), newParent ? newParent->GetFullName() : "nil"
        );
    
    if (newParent.Reference.TargetId != this->ObjectId)
    {
        if (newParent && newParent->IsDescendantOf(this))
            RAISE_RT(
                "Tried to make {} a descendant of itself",
                GetFullName()
            );
    }
    else
    {
        RAISE_RT(
            "Tried to make {} its own parent",
            GetFullName()
        );
    }

    if (newParent.Reference.TargetId == Parent)
        return;

    GameObject* oldParent = GameObjectManager::Get()->FindById(Parent);

    // we HAVE to do this BEFORE `::RemoveChild`, otherwise
    // it could get called twice in a row due to `::DecrementHardRefs`
    // leading to a `::Destroy`
    Parent = PHX_GAMEOBJECT_NULL_ID;

    if (oldParent)
    {
        oldParent->RemoveChild(this->ObjectId);

        if (!newParent)
            DecrementHardRefs();
    }
    else
        IncrementHardRefs();

    if (History* history = History::Get(); history->IsRecordingEnabled)
    {
        if (history->TargetDataModel == OwningDataModel || (newParent && (history->TargetDataModel == newParent->ObjectId || history->TargetDataModel == newParent->OwningDataModel)))
        {
            history->RecordEvent({
                .Target = { .Id = ObjectId, .Type = EntityComponent::None },
                .TargetObject = this,
                .Property = &s_Api.Properties.at("Parent"),
                .PreviousValue = GameObject::s_ToGenericValue(oldParent),
                .NewValue = GameObject::s_ToGenericValue(newParent.Reference.Referred())
            });
        }
    }

    if (newParent)
    {
        this->Parent = newParent->ObjectId;
        newParent->AddChild(this);
    }

    EvaluateOwners();
}

void GameObject::AddChild(const ObjectHandle& c)
{
    ZoneScoped;

    if (c->ObjectId == this->ObjectId)
        RAISE_RT(
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
        //if (GameObject* ch = GameObjectManager::Get()->FindById(id))
        //	ch->DecrementHardRefs();
    }
    else
        RAISE_RT("ID:{} is _not my ({}) sonnn~_", ObjectId, id);
}

bool GameObject::GetEnabled() const
{
    return m_Enabled;
}

void GameObject::SetEnabled(bool Enabled)
{
    m_Enabled = Enabled;
    bool wasTreeEnabled = TreeEnabled;

    if (m_Enabled)
    {
        if (GameObject* parent = GetParent())
            TreeEnabled = parent->TreeEnabled;
        else
            TreeEnabled = true;
    }
    else
        TreeEnabled = false;

    if (wasTreeEnabled != TreeEnabled)
    {
        ForEachChild([](const ObjectHandle& child) -> bool
        {
            child->SetEnabled(child->GetEnabled());
            return true;
        });

        Reflection::SignalEvent(OnTreeEnabledChangedCallbacks, { TreeEnabled }, "GameObject.OnTreeEnabledChanged");
    }

    if (EcSound* sound = FindComponent<EcSound>())
        sound->Update(0.0);
}

Reflection::GenericValue GameObject::s_ToGenericValue(GameObject* Object)
{
    Reflection::GenericValue gv = Object ? Object->ObjectId : PHX_GAMEOBJECT_NULL_ID;
    gv.Type = Reflection::ValueType::GameObject;

    if (Object)
    {
        // Decremented by destructor of `GenericValue`
        // TODO GVOBJECT
        Object->IncrementHardRefs();
    }

    return gv;
}

Reflection::GenericValue GameObject::ToGenericValue()
{
    assert(this);
    return s_ToGenericValue(this);
}

GameObject* GameObject::GetParent() const
{
    return GameObjectManager::Get()->FindById(this->Parent);
}

void GameObject::ForEachChild(const std::function<bool(const ObjectHandle&)>& Callback)
{
    ZoneScoped;

    GameObjectManager* ObjectManager = GameObjectManager::Get();
    size_t beforeSize = ObjectManager->WorldArray.size();

    for (uint32_t id : Children)
    {
        GameObject* child = GameObjectManager::Get()->FindById(id);
        assert(child);

        if (child->Parent != ObjectId)
            continue; // callback changed parents

        if (bool shouldContinue = Callback(child); !shouldContinue)
            break;

        if (ObjectManager->WorldArray.size() != beforeSize)
            RAISE_RT("Callback cannot create objects"); // `this` may get re-allocated, very very bad
    }
}

bool GameObject::ForEachDescendant(const std::function<bool(const ObjectHandle&)>& Callback)
{
    ZoneScoped;

    GameObjectManager* ObjectManager = GameObjectManager::Get();
    size_t beforeSize = ObjectManager->WorldArray.size();

    for (uint32_t id : Children)
    {
        GameObject* child = GameObjectManager::Get()->FindById(id);
        assert(child);

        if (child->Parent != ObjectId)
            continue; // callback changed parents

        if (bool shouldContinue = Callback(child); !shouldContinue)
            return true;

        if (ObjectManager->WorldArray.size() != beforeSize)
            RAISE_RT("Callback cannot create objects"); // `this` may get re-allocated, very very bad

        if (child->ForEachDescendant(Callback))
            return true;
    }

    return false;
}

std::vector<ObjectHandle> GameObject::GetChildren() const
{
    std::vector<ObjectHandle> children;
    children.reserve(Children.size());

    for (uint32_t id : Children)
    {
        GameObject* child = GameObjectManager::Get()->FindById(id);
        assert(child);
        children.emplace_back(child);
    }

    return children;
}

std::vector<ObjectHandle> GameObject::GetDescendants() const
{
    ZoneScoped;

    std::vector<ObjectHandle> descendants;
    descendants.reserve(Children.size());

    for (uint32_t id : Children)
    {
        GameObject* child = GameObjectManager::Get()->FindById(id);
        assert(child);
        descendants.emplace_back(child);

        std::vector<ObjectHandle> childrenDescendants = child->GetDescendants();
        std::copy(childrenDescendants.begin(), childrenDescendants.end(), std::back_inserter(descendants));
    }

    return descendants;
}

GameObject* GameObject::FindChild(const std::string_view& TargetName) const
{
    for (uint32_t id : Children)
    {
        GameObject* child = GameObjectManager::Get()->FindById(id);
        assert(child);

        if (child->Name == TargetName)
            return child;
    }

    return nullptr;
}


GameObject* GameObject::FindChildWithComponent(EntityComponent Component) const
{
    for (uint32_t id : Children)
    {
        GameObject* child = GameObjectManager::Get()->FindById(id);
        assert(child);

        if (child->FindComponentByType(Component))
            return child;
    }

    return nullptr;
}

uint32_t GameObject::AddComponent(EntityComponent Type)
{
    PHX_ENSURE(Valid);
    assert(Type != EntityComponent::None);

    if (FindComponentByType(Type))
        RAISE_RT("Already have that component");

    IComponentManager* manager = GetComponentManagerByComponentType(Type);
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

    if (History* history = History::Get(); history->IsRecordingEnabled)
    {
        if (OwningDataModel == history->TargetDataModel)
            history->RecordEvent({
                .Target = { .Id = ObjectId },
                .TargetObject = this,
                .Property = std::nullopt,
                .PreviousValue = Reflection::GenericValue::Null(),
                .NewValue = (int64_t)Type
            });
    }

    return componentId;
}

void GameObject::RemoveComponent(EntityComponent Type)
{
    assert(Type != EntityComponent::None);

    for (auto vit = Components.begin(); vit < Components.end(); vit++)
    {
        if (vit->Type == Type)
        {
            IComponentManager* manager = GetComponentManagerByComponentType(Type);
            ReflectorRef ref = *vit;

            if (History* history = History::Get(); history->IsRecordingEnabled && OwningDataModel == history->TargetDataModel)
            {
                for (const auto& [ name, prop ] : manager->GetProperties())
                {
                    if (!prop.Serializes || !prop.Set)
                        continue;

                    Reflection::GenericValue val = GetPropertyValue(name);

                    // We need to keep track of all of the properties to restore
                    // when Undo'ing
                    history->RecordEvent({
                        .Target = { .Id = ObjectId },
                        .TargetObject = this,
                        .Property = &prop,
                        .PreviousValue = val,
                        .NewValue = val
                    });
                }

                history->RecordEvent({
                    .Target = { .Id = ObjectId },
                    .TargetObject = this,
                    .Property = std::nullopt,
                    .PreviousValue = (int64_t)Type,
                    .NewValue = Reflection::GenericValue::Null()
                });
            }

            Components.erase(vit);
            manager->DeleteComponent(ref.Id);

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
    }

    RAISE_RT("Tried to remove {} component from {}, but it doesn't have that!", s_EntityComponentNames[(uint8_t)Type], GetFullName());
}

const Reflection::PropertyDescriptor* GameObject::FindProperty(const std::string_view& PropName, ReflectorRef* Reflector) const
{
    ReflectorRef dummyRef = {};
    Reflector = Reflector ? Reflector : &dummyRef;

    if (auto it = s_Api.Properties.find(PropName); it != s_Api.Properties.end())
    {
        Reflector->Id = ObjectId;
        return &it->second;
    }

    if (auto it = ComponentApis.Properties.find(PropName); it != ComponentApis.Properties.end())
    {
        const auto& fcit = MemberToComponentMap.find(PropName);
        assert(fcit != MemberToComponentMap.end());
        *Reflector = fcit->second;

        assert(Reflector->Referred());
        return it->second;
    }

    return nullptr;
}

const Reflection::MethodDescriptor* GameObject::FindMethod(const std::string_view& FuncName, ReflectorRef* Reflector) const
{
    ReflectorRef dummyRef = {};
    Reflector = Reflector ? Reflector : &dummyRef;

    if (auto it = s_Api.Methods.find(FuncName); it != s_Api.Methods.end())
    {
        Reflector->Id = ObjectId;
        return &it->second;
    }

    if (auto it = ComponentApis.Methods.find(FuncName); it != ComponentApis.Methods.end())
    {
        const auto& fcit = MemberToComponentMap.find(FuncName);
        assert(fcit != MemberToComponentMap.end());
        *Reflector = fcit->second;

        assert(Reflector->Referred());
        return it->second;
    }

    return nullptr;
}

const Reflection::EventDescriptor* GameObject::FindEvent(const std::string_view& EventName, ReflectorRef* Reflector) const
{
    ReflectorRef dummyRef = {};
    Reflector = Reflector ? Reflector : &dummyRef;

    if (auto it = s_Api.Events.find(EventName); it != s_Api.Events.end())
    {
        Reflector->Id = ObjectId;
        return &it->second;
    }

    if (auto it = ComponentApis.Events.find(EventName); it != ComponentApis.Events.end())
    {
        const auto& fcit = MemberToComponentMap.find(EventName);
        assert(fcit != MemberToComponentMap.end());
        *Reflector = fcit->second;

        assert(Reflector->Referred());
        return it->second;
    }

    return nullptr;
}

Reflection::GenericValue GameObject::GetPropertyValue(const std::string_view& PropName) const
{
    ReflectorRef ref;

    if (const Reflection::PropertyDescriptor* prop = FindProperty(PropName, &ref))
    {
        Reflection::GenericValue gv = prop->Get(ref.Referred());
        assert(Reflection::TypeFits(prop->Type, gv.Type));

        return prop->Get(ref.Referred());
    }

    RAISE_RT("Invalid property '{}' in GetPropertyValue", PropName);
}

void GameObject::SetPropertyValue(const std::string_view& PropName, const Reflection::GenericValue& Value)
{
    ZoneScoped;
    ZoneText(PropName.data(), PropName.size());

    ReflectorRef ref;

    if (const Reflection::PropertyDescriptor* prop = FindProperty(PropName, &ref))
    {
        if (!prop->Set)
            RAISE_RT("{} of {} is read-only!", PropName, GetFullName());

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

    RAISE_RT("Invalid property '{}' in SetPropertyValue", PropName);
}

Reflection::GenericValue GameObject::GetDefaultPropertyValue(const std::string_view& PropName) const
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
            return GetComponentManagerByComponentType(ref.Type)->GetDefaultPropertyValue(PropName);
        }
    }

    RAISE_RT("Invalid property '{}' in GetDefaultPropertyValue", PropName);
}

std::vector<Reflection::GenericValue> GameObject::CallMethod(const std::string_view& FuncName, const std::vector<Reflection::GenericValue>& Inputs)
{
    ReflectorRef ref;

    if (const Reflection::MethodDescriptor* func = FindMethod(FuncName, &ref))
        return func->Function(ref.Referred(), Inputs);

    RAISE_RT("Invalid function '{}' in CallMethod", FuncName);
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

void* GameObject::FindComponentByType(EntityComponent Type) const
{
    assert(Type != EntityComponent::None);

    for (const ReflectorRef& ref : Components)
        if (ref.Type == Type)
            return GetComponentManagerByComponentType(Type)->GetComponent(ref.Id);
    
    return nullptr;
}

ObjectHandle GameObjectManager::Create()
{
    if (WorldArray.size() == 0)
    {
        WorldArray.emplace_back();
        WorldArray[0].Name = "<RESERVED INVALID SLOT>";
        WorldArray[0].Valid = false;
    }

    uint32_t id = NextFreeId;

    if (id == UINT32_MAX)
    {
        id = static_cast<uint32_t>(WorldArray.size());

        if (id >= UINT32_MAX - 1)
            RAISE_RT("Reached end of GameObject ID space (2^32 - 1)");

#ifndef NDEBUG
    // cause as many re-allocations as possible to catch stale pointers
    WorldArray.shrink_to_fit();
#endif

        WorldArray.emplace_back();
    }
    else
    {
        GameObject& reused = WorldArray[id];
        NextFreeId = reused.NextFreeId; // stole this trick from `lua_ref`

        reused.~GameObject();
        new (&reused) GameObject();
    }

    GameObject& created = WorldArray[id];
    created.ObjectId = id;
    return &created;
}

ObjectHandle GameObjectManager::Create(EntityComponent FirstComponent)
{
    ObjectHandle created = Create();
    created->AddComponent(FirstComponent);
    created->Name = s_EntityComponentNames[(size_t)FirstComponent];

    return created;
}

ObjectHandle GameObjectManager::Create(const std::string_view& FirstComponent)
{
    EntityComponent it = FindComponentTypeByName(FirstComponent);

    if (it == EntityComponent::None)
        RAISE_RT("Invalid Component Name '{}'", FirstComponent);
    else
        return Create(it);
}

GameObjectManager::Collection& GameObjectManager::GetCollection(const std::string& Name)
{
    const auto& it = CollectionNameToId.find(Name);

    if (it == CollectionNameToId.end())
    {
        if (Collections.size() >= UINT16_MAX)
            RAISE_RT("Cannot create collection '{}' because we have reached the limit of 2^16", Name);

        uint16_t id = static_cast<uint16_t>(Collections.size());
        CollectionNameToId[Name] = id;
        Collections.push_back(Collection{
            .Name = Name,
            .AddedEvent = { .Descriptor = new Reflection::EventDescriptor{
                .CallbackInputs = REFLECTION_SPAN({ Reflection::ValueType::GameObject }),
                .Connect = [this, id](void*, Reflection::EventConnection Callback) -> uint32_t
                {
                    return Reflection::EventConnect(Collections[id].AddedEvent.Callbacks, Callback);
                },
                .Disconnect = [this, id](void*, uint32_t ConnectionId) noexcept
                {
                    Reflection::EventDisconnect(Collections[id].AddedEvent.Callbacks, ConnectionId);
                },
                .Cleanup = [this, id](void*)
                {
                    Reflection::EventCleanup(Collections[id].AddedEvent.Callbacks);
                },
            } },
            .RemovedEvent = { .Descriptor = new Reflection::EventDescriptor{
                .CallbackInputs = REFLECTION_SPAN({ Reflection::ValueType::GameObject }),
                .Connect = [this, id](void*, Reflection::EventConnection Callback) -> uint32_t
                {
                    return Reflection::EventConnect(Collections[id].RemovedEvent.Callbacks, Callback);
                },
                .Disconnect = [this, id](void*, uint32_t ConnectionId) noexcept
                {
                    Reflection::EventDisconnect(Collections[id].RemovedEvent.Callbacks, ConnectionId);
                },
                .Cleanup = [this, id](void*)
                {
                    Reflection::EventCleanup(Collections[id].RemovedEvent.Callbacks);
                },
            } },
            .Id = id,
        });

        return Collections[id];
    }
    else
        return Collections[it->second];
}

void GameObject::AddTag(const std::string& Tag)
{
    if (IsDestructionPending)
        RAISE_RT("Cannot call `:AddTag` on Object {} which was destroyed", GetFullName());

    GameObjectManager::Collection& collection = GameObjectManager::Get()->GetCollection(Tag);
    bool alreadyHave = std::find(Tags.begin(), Tags.end(), collection.Id) != Tags.end();

    if (!alreadyHave)
    {
        Tags.push_back(collection.Id);
        collection.Items.push_back(ObjectId);

        Reflection::SignalEvent(collection.AddedEvent.Callbacks, { this->ToGenericValue() }, "TagAddedSignal");
        Reflection::SignalEvent(OnTagAddedCallbacks, { Tag }, "GameObject.OnTagAdded");

        if (History* history = History::Get(); history->IsRecordingEnabled)
        {
            if (OwningDataModel == history->TargetDataModel)
                history->RecordEvent({
                    .Target = { .Id = ObjectId },
                    .TargetObject = this,
                    .Property = std::nullopt,
                    .PreviousValue = Reflection::GenericValue::Null(),
                    .NewValue = Tag,
                    .IsTag = true
                });
        }
    }
}

void GameObject::RemoveTag(const std::string& Tag)
{
    GameObjectManager* ObjectManager = GameObjectManager::Get();
    const auto& it = ObjectManager->CollectionNameToId.find(Tag);

    if (it == ObjectManager->CollectionNameToId.end())
        return;

    const auto& tagIt = std::find(Tags.begin(), Tags.end(), it->second);
    if (tagIt != Tags.end())
    {
        Tags.erase(tagIt);

        GameObjectManager::Collection& collection = ObjectManager->Collections[it->second];
        collection.Items.erase(std::find(collection.Items.begin(), collection.Items.end(), ObjectId));
        Reflection::SignalEvent(collection.RemovedEvent.Callbacks, { this->ToGenericValue() }, "OnTagRemovedSignal");
        Reflection::SignalEvent(OnTagRemovedCallbacks, { Tag }, "GameObject.OnTagRemoved");

        if (History* history = History::Get(); history->IsRecordingEnabled)
        {
            if (OwningDataModel == history->TargetDataModel)
                history->RecordEvent({
                    .Target = { .Id = ObjectId },
                    .TargetObject = this,
                    .Property = std::nullopt,
                    .PreviousValue = Tag,
                    .NewValue = Reflection::GenericValue::Null(),
                    .IsTag = true
                });
        }
    }
}

static GameObjectManager* ObjectManagerInstance = nullptr;

GameObjectManager::GameObjectManager()
{
    assert(!ObjectManagerInstance);
    ObjectManagerInstance = this;
}

GameObjectManager::~GameObjectManager()
{
    assert(ObjectManagerInstance);
    ObjectManagerInstance = nullptr;
}

GameObjectManager* GameObjectManager::Get()
{
    assert(ObjectManagerInstance);
    return ObjectManagerInstance;
}

bool GameObject::HasTag(const std::string& Tag) const
{
    GameObjectManager* ObjectManager = GameObjectManager::Get();
    const auto& it = ObjectManager->CollectionNameToId.find(Tag);

    if (it == ObjectManager->CollectionNameToId.end())
        return false;

    const auto& tagIt = std::find(Tags.begin(), Tags.end(), it->second);
    return tagIt != Tags.end();
}

static void dumpProperties(const Reflection::StaticPropertyMap Properties, nlohmann::json& Json)
{
    for (const auto& propIt : Properties)
    {
        std::string pstr = Reflection::TypeAsString(propIt.second.Type);

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

        for (Reflection::ValueType i : funcIt.second.Parameters)
            istring += std::string(Reflection::TypeAsString(i)) + ", ";
        
        for (Reflection::ValueType o : funcIt.second.Returns)
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
    
    for (size_t i = 0; i < (size_t)EntityComponent::count; i++)
    {
        IComponentManager* manager = GameObjectManager::Get()->ComponentManagers[i];

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
