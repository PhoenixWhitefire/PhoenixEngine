// Collections service - tagging objects, 31/01/2026
#include "component/Collections.hpp"
#include "datatype/GameObject.hpp"

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
                    .Name = "CollectionsService:GetTagAddedSignal() -> Event"
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
                    .Name = "CollectionsService:GetTagRemovedSignal() -> Event"
                };
                gv.Type = Reflection::ValueType::EventSignal;

                return { gv };
            }
        } }
    };

    return methods;
}
