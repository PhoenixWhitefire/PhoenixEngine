// Collections service - tagging objects, 31/01/2026
#include "component/Collections.hpp"
#include "datatype/GameObject.hpp"

uint32_t CollectionsComponentManager::CreateComponent(GameObject* Object)
{
    uint32_t id = ComponentManager<EcCollections>::CreateComponent(Object);
    m_Components[id].Reference = { id, EntityComponent::Collections };
    m_Components[id].Object = Object;

    return id;
}

const Reflection::StaticMethodMap& CollectionsComponentManager::GetMethods()
{
    static const Reflection::StaticMethodMap methods = {
        { "GetCollections", Reflection::MethodDescriptor{
            {},
            { Reflection::ValueType::Array },
            [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                std::vector<Reflection::GenericValue> tags;
                for (const auto& it : GameObjectManager::Get()->CollectionNameToId)
                    tags.emplace_back(it.first);

                return { Reflection::GenericValue(tags) };
            }
        } },

        { "GetTagged", Reflection::MethodDescriptor{
            { Reflection::ValueType::String },
            { Reflection::ValueType::Array },
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                GameObjectManager* ObjectManager = GameObjectManager::Get();

                std::vector<Reflection::GenericValue> tagged;

                const auto& it = ObjectManager->CollectionNameToId.find(inputs[0].AsString());
                if (it == ObjectManager->CollectionNameToId.end())
                    return { Reflection::GenericValue(std::vector<Reflection::GenericValue>()) };

                for (uint32_t objectId : ObjectManager->Collections[it->second].Items)
                {
                    GameObject* object = ObjectManager->FindById(objectId);

                    if (object && !object->IsDestructionPending)
                        tagged.push_back(object->ToGenericValue());
                }

                return { Reflection::GenericValue(tagged) };
            }
        } },

        { "GetTagAddedSignal", Reflection::MethodDescriptor{
            { Reflection::ValueType::String },
            { Reflection::ValueType::EventSignal },
            [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                const std::string tag = inputs[0].AsString();
                const GameObjectManager::Collection& collection = GameObjectManager::Get()->GetCollection(tag);

                Reflection::GenericValue gv;
                gv.Val.Event = {
                    .Descriptor = collection.AddedEvent.Descriptor,
                    .Reflector = static_cast<EcCollections*>(p)->Reference,
                    .Name = "Collections:GetTagAddedSignal() -> Event",
                };

                gv.Type = Reflection::ValueType::EventSignal;
                return { gv };
            }
        } },

        { "GetTagRemovedSignal", Reflection::MethodDescriptor{
            { Reflection::ValueType::String },
            { Reflection::ValueType::EventSignal },
            [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                const std::string tag = inputs[0].AsString();
                const GameObjectManager::Collection& collection = GameObjectManager::Get()->GetCollection(tag);

                Reflection::GenericValue gv;
                gv.Val.Event = {
                    .Descriptor = collection.RemovedEvent.Descriptor,
                    .Reflector = static_cast<EcCollections*>(p)->Reference,
                    .Name = "Collections:GetTagRemovedSignal() -> Event",
                };

                gv.Type = Reflection::ValueType::EventSignal;
                return { gv };
            }
        } },

        { "GetObjectsWithComponent", Reflection::MethodDescriptor{
            { Reflection::ValueType::String, REFLECTION_OPTIONAL(GameObject) },
            { Reflection::ValueType::Array },
            [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                const EcCollections* collections = static_cast<EcCollections*>(p);
                EntityComponent ec = FindComponentTypeByName(inputs[0].AsStringView());
                uint32_t datamodel = inputs.size() > 1 && inputs[1].Type != Reflection::ValueType::Null
                                        ? GameObjectManager::Get()->FromGenericValue(inputs[1])->ObjectId
                                        : collections->Object->OwningDataModel;

                std::vector<Reflection::GenericValue> ret;

                GetComponentManagerByComponentType(ec)->ForEachComponent([&ret, datamodel](BaseComponent* component) -> bool
                {
                    if (component->Object->OwningDataModel == datamodel)
                        ret.push_back(component->Object->ToGenericValue());

                    return true;
                });

                return { Reflection::GenericValue(ret) };
            }
        } },

        { "GetComponentCreatedSignal", Reflection::MethodDescriptor{
            { Reflection::ValueType::String, REFLECTION_OPTIONAL(GameObject) },
            { Reflection::ValueType::EventSignal },
            [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                const EcCollections* collections = static_cast<EcCollections*>(p);
                GameObjectManager* objectManager = GameObjectManager::Get();

                EntityComponent ec = FindComponentTypeByName(inputs[0].AsStringView());
                uint32_t datamodelId = inputs.size() > 1 && inputs[1].Type != Reflection::ValueType::Null
                                            ? objectManager->FromGenericValue(inputs[1])->ObjectId
                                            : collections->Object->OwningDataModel;

                if (ec == EntityComponent::None)
                    RAISE_RT("Invalid component type '{}'", inputs[0].AsStringView());

                IComponentManager* cm = GetComponentManagerByComponentType(ec);

                Reflection::GenericValue gv;
                gv.Val.Event = {
                    .Descriptor = &cm->ComponentCreatedEvent,
                    .Reflector = static_cast<EcCollections*>(p)->Reference,
                    .Name = "Collections:GetComponentCreatedSignal() -> Event",
                    .RestrictDataModel = datamodelId,
                };

                gv.Type = Reflection::ValueType::EventSignal;
                return { gv };
            }
        } },

        { "GetComponentDeletedSignal", Reflection::MethodDescriptor{
            { Reflection::ValueType::String, REFLECTION_OPTIONAL(GameObject) },
            { Reflection::ValueType::EventSignal },
            [](void* p, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                const EcCollections* collections = static_cast<EcCollections*>(p);
                GameObjectManager* objectManager = GameObjectManager::Get();

                EntityComponent ec = FindComponentTypeByName(inputs[0].AsStringView());
                uint32_t datamodelId = inputs.size() > 1 && inputs[1].Type != Reflection::ValueType::Null
                                            ? objectManager->FromGenericValue(inputs[1])->ObjectId
                                            : collections->Object->OwningDataModel;

                if (ec == EntityComponent::None)
                    RAISE_RT("Invalid component type '{}'", inputs[0].AsStringView());

                IComponentManager* cm = GetComponentManagerByComponentType(ec);

                Reflection::GenericValue gv;
                gv.Val.Event = {
                    .Descriptor = &cm->ComponentDeletedEvent,
                    .Reflector = static_cast<EcCollections*>(p)->Reference,
                    .Name = "Collections:GetComponentDeletedSignal() -> Event",
                    .RestrictDataModel = datamodelId,
                };

                gv.Type = Reflection::ValueType::EventSignal;
                return { gv };
            }
        } },
    };

    return methods;
}
