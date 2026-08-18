// HistoryService.hpp, 17/12/2025 - Interfacing with History backend

#include "component/HistoryService.hpp"
#include "History.hpp"

static Reflection::GenericValue dumpActionData(const History::Action& action)
{
    std::vector<Reflection::GenericValue> eventsVec;
    eventsVec.reserve(action.Events.size());

    for (const History::Event& event : action.Events)
    {
        std::vector<Reflection::GenericValue> eventData;
        eventData.reserve(4);

        if (event.Property.has_value())
        {
            eventsVec.push_back(Reflection::GenericValue::MapPairs({
                { "Target", event.TargetObject->ToGenericValue() },
                { "Type", "PropertyChanged" },
                { "Property", event.Property.value()->Name },
                { "NewValue", event.NewValue },
                { "PreviousValue", event.PreviousValue },
            }));
        }
        else
        {
            if (event.PreviousValue.IsNull())
            {
                assert(event.PreviousValue.IsNull());

                eventsVec.push_back(Reflection::GenericValue::MapPairs({
                    { "Target", event.TargetObject->ToGenericValue() },
                    { "Type", event.IsTag ? "Tag" : "Component" },
                    { "Action", "Add" },
                    { event.IsTag ? "Tag" : "Component", event.IsTag ? event.NewValue.AsString() : s_EntityComponentNames[event.NewValue.AsInteger()] },
                }));
            }
            else
            {
                assert(event.NewValue.IsNull());

                eventsVec.push_back(Reflection::GenericValue::MapPairs({
                    { "Target", event.TargetObject->ToGenericValue() },
                    { "Type", event.IsTag ? "Tag" : "Component" },
                    { "Action", "Remove" },
                    { event.IsTag ? "Tag" : "Component", event.IsTag ? event.PreviousValue.AsString() : s_EntityComponentNames[event.PreviousValue.AsInteger()] },
                }));
            }
        }
    }

    Reflection::GenericValue actionValue = Reflection::GenericValue::MapPairs({
        { "Name", action.Name },
        { "Events", eventsVec },
    });
    return { actionValue };
}

const Reflection::StaticPropertyMap& HistoryComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props = {
        REFLECTION_PROPERTY(
            "RecordingEnabled",
            Boolean,
            [](void*) -> Reflection::GenericValue
            {
                History* history = History::Get();
                return history->IsRecordingEnabled;
            },
            [](void*, const Reflection::GenericValue& gv)
            {
                History* history = History::Get();
                history->IsRecordingEnabled = gv.AsBoolean();
            }
        ),

        REFLECTION_PROPERTY(
            "CurrentWaypoint",
            Integer,
            [](void*) -> Reflection::GenericValue
            {
                History* history = History::Get();
                return (int64_t)history->GetCurrentWaypoint();
            },
            nullptr
        ),

        { "CurrentActionName", Reflection::PropertyDescriptor{
            .Name = "CurrentActionName",
            .Get = [](void*) -> Reflection::GenericValue
            {
                History* history = History::Get();
                const std::optional<History::Action>& currentAction = history->GetCurrentAction();

                if (currentAction.has_value())
                    return currentAction->Name;
                else
                    return {};
            },
            .Set = nullptr,
            .Type = REFLECTION_OPTIONAL(String),
        } },

        REFLECTION_PROPERTY(
            "ActionHistorySize",
            Integer,
            [](void*) -> Reflection::GenericValue
            {
                History* history = History::Get();
                return (int64_t)history->GetActionHistory().size();
            },
            nullptr
        ),

        REFLECTION_PROPERTY(
            "IsRecordingAction",
            Boolean,
            [](void*) -> Reflection::GenericValue
            {
                History* history = History::Get();
                return history->GetCurrentAction().has_value();
            },
            nullptr
        ),

        REFLECTION_PROPERTY(
            "TargetDataModel",
            GameObject,
            [](void*) -> Reflection::GenericValue
            {
                History* history = History::Get();
                return GameObject::s_ToGenericValue(GameObjectManager::Get()->FindById(history->TargetDataModel));
            },
            [](void*, const Reflection::GenericValue& gv)
            {
                History* history = History::Get();
                GameObject* newTarget = GameObjectManager::Get()->FromGenericValue(gv);

                if (!newTarget->FindComponentByType(EntityComponent::DataModel))
                    RAISE_RT("Object {} is not a DataModel!", newTarget->GetFullName());

                history->TargetDataModel = newTarget->ObjectId;
            }
        ),

        REFLECTION_PROPERTY(
            "CanUndo",
            Boolean,
            [](void*) -> Reflection::GenericValue
            {
                History* history = History::Get();
                return history->CanUndo();
            },
            nullptr
        ),
        REFLECTION_PROPERTY(
            "CanRedo",
            Boolean,
            [](void*) -> Reflection::GenericValue
            {
                History* history = History::Get();
                return history->CanRedo();
            },
            nullptr
        ),
    };

    return props;
}

const Reflection::StaticMethodMap& HistoryComponentManager::GetMethods()
{
    static const Reflection::StaticMethodMap methods = {
        { "TryBeginAction", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::String }),
            REFLECTION_SPAN({ REFLECTION_OPTIONAL(Integer) }),
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                History* history = History::Get();
                std::optional<size_t> actionId = history->TryBeginAction(inputs[0].AsString());

                if (!actionId)
                    return { Reflection::GenericValue::Null() };
                else
                    return { (uint32_t)actionId.value() };
            }
        } },

        { "FinishAction", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::Integer }),
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                History* history = History::Get();
                history->FinishAction((size_t)inputs[0].AsInteger());

                return {};
            }
        } },

        { "DiscardAction", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::Integer }),
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                History* history = History::Get();
                history->DiscardAction((size_t)inputs[0].AsInteger());

                return {};
            }
        } },

        { "ClearHistory", Reflection::MethodDescriptor{
            {},
            {},
            [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                History* history = History::Get();
                history->ClearHistory();

                return {};
            }
        } },

        { "GetCannotUndoReason", Reflection::MethodDescriptor{
            {},
            REFLECTION_SPAN({ Reflection::ValueType::String }),
            [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                History* history = History::Get();
                return { history->GetCannotUndoReason() };
            }
        } },

        { "GetCannotRedoReason", Reflection::MethodDescriptor{
            {},
            REFLECTION_SPAN({ Reflection::ValueType::String }),
            [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                History* history = History::Get();
                return { history->GetCannotRedoReason() };
            }
        } },

        { "Undo", Reflection::MethodDescriptor{
            {},
            {},
            [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                History* history = History::Get();
                history->Undo();

                return {};
            }
        } },

        { "Redo", Reflection::MethodDescriptor{
            {},
            {},
            [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                History* history = History::Get();
                history->Redo();

                return {};
            }
        } },

        { "GetCurrentActionData", Reflection::MethodDescriptor{
            {},
            REFLECTION_SPAN({ REFLECTION_OPTIONAL(Map) }),
            [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                History* history = History::Get();
                const std::optional<History::Action>& action = history->GetCurrentAction();

                if (!action.has_value())
                    return { Reflection::GenericValue::Null() };

                return { dumpActionData(*action) };
            }
        } },

        { "GetActionData", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::Integer }),
            REFLECTION_SPAN({ Reflection::ValueType::Map }),
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                History* history = History::Get();
                const History::Action& action = history->GetActionHistory().at(inputs[0].AsInteger());

                return { dumpActionData(action) };
            }
        } },
    };

    return methods;
}
