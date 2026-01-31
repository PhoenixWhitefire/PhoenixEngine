// Collections service - tagging objects, 31/01/2026
#include "component/Collections.hpp"
#include "datatype/GameObject.hpp"

class CollectionsServiceManager : public ComponentManager<EcCollections>
{
public:
    virtual const Reflection::StaticMethodMap& GetMethods() override
    {
        static const Reflection::StaticMethodMap methods = {
            { "GetCollections", Reflection::MethodDescriptor{
                {},
                { Reflection::ValueType::Array },
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    std::vector<Reflection::GenericValue> tags;
                    for (const auto& it : GameObject::s_CollectionNameToId)
                        tags.emplace_back(it.first);

                    return { Reflection::GenericValue(tags) };
                }
            } },

            { "GetTagged", Reflection::MethodDescriptor{
                { Reflection::ValueType::String },
                { Reflection::ValueType::Array },
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    std::vector<Reflection::GenericValue> tagged;

                    const auto& it = GameObject::s_CollectionNameToId.find(inputs[0].AsString());
                    if (it == GameObject::s_CollectionNameToId.end())
                        return { Reflection::GenericValue(std::vector<Reflection::GenericValue>()) };

                    for (uint32_t objectId : GameObject::s_Collections[it->second].Items)
                    {
                        GameObject* object = GameObject::GetObjectById(objectId);

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
                    const auto& it = GameObject::s_CollectionNameToId.find(inputs[0].AsString());
                    GameObject::Collection* collection = nullptr;

                    if (it == GameObject::s_CollectionNameToId.end())
                    {
                        // TODO hack
                        // create and initialize the collection
                        GameObject* dm = GameObject::GetObjectById(GameObject::s_DataModel);
                        dm->AddTag(inputs[0].AsString());
                        dm->RemoveTag(inputs[0].AsString());

                        collection = &GameObject::s_Collections[GameObject::s_CollectionNameToId.at(inputs[0].AsString())];
                    }
                    else
                    {
                        collection = &GameObject::s_Collections[it->second];
                    }

                    Reflection::GenericValue gv;
                    gv.Val.Event = {
                        .Descriptor = collection->AddedEvent.Descriptor,
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
                    const auto& it = GameObject::s_CollectionNameToId.find(inputs[0].AsString());
                    GameObject::Collection* collection = nullptr;

                    if (it == GameObject::s_CollectionNameToId.end())
                    {
                        // TODO hack
                        // create and initialize the collection
                        GameObject* dm = GameObject::GetObjectById(GameObject::s_DataModel);
                        dm->AddTag(inputs[0].AsString());
                        dm->RemoveTag(inputs[0].AsString());

                        collection = &GameObject::s_Collections[GameObject::s_CollectionNameToId.at(inputs[0].AsString())];
                    }
                    else
                    {
                        collection = &GameObject::s_Collections[it->second];
                    }

                    Reflection::GenericValue gv;
                    gv.Val.Event = {
                        .Descriptor = collection->RemovedEvent.Descriptor,
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
};

static CollectionsServiceManager Instance;
