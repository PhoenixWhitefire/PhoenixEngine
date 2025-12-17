// HistoryService.hpp, 17/12/2025 - Interfacing with History backend

#include "component/HistoryService.hpp"
#include "History.hpp"

static Reflection::GenericValue dumpActionData(const History::Action& action)
{
    std::vector<Reflection::GenericValue> actionValueVec;
    actionValueVec.reserve(4);

    actionValueVec.emplace_back("Name");
    actionValueVec.emplace_back(action.Name);

    std::vector<Reflection::GenericValue> eventsVec;
    eventsVec.reserve(action.Events.size());

    for (const History::PropertyEvent& event : action.Events)
    {
        std::vector<Reflection::GenericValue> eventData;
        eventData.reserve(4);

        eventData.emplace_back("Target");
        eventData.emplace_back(event.TargetObject->ToGenericValue());

        eventData.emplace_back("Property");
        eventData.emplace_back(event.Property->Name);

        eventData.emplace_back("NewValue");
        eventData.push_back(event.NewValue);

        eventData.emplace_back("PreviousValue");
        eventData.push_back(event.PreviousValue);

        Reflection::GenericValue eventValue = { eventData };
        eventValue.Type = Reflection::ValueType::Map;

        eventsVec.emplace_back(eventValue);
    }

    actionValueVec.emplace_back("Events");
    actionValueVec.emplace_back(eventsVec);

    Reflection::GenericValue actionValue = { actionValueVec };
    actionValue.Type = Reflection::ValueType::Map;
    return { actionValue };
}

class HistoryServiceManager : ComponentManager<EcHistoryService>
{
public:
    const Reflection::StaticPropertyMap& GetProperties() override
    {
        static const Reflection::StaticPropertyMap props = {
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

            { "CurrentActionName", {
                "CurrentActionName",
                REFLECTION_OPTIONAL(String),
                [](void*) -> Reflection::GenericValue
                {
                    History* history = History::Get();
                    const std::optional<History::Action>& currentAction = history->GetCurrentAction();

                    if (currentAction.has_value())
                        return currentAction->Name;
                    else
                        return {};
                },
                nullptr
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
            )
        };

        return props;
    }

    const Reflection::StaticMethodMap& GetMethods() override
    {
        static const Reflection::StaticMethodMap methods = {
            { "EnableRecording", {
                {},
                {},
                [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
                {
                    History* history = History::Get();
                    history->IsRecordingEnabled = true;

                    return {};
                }
            } },

            { "TryBeginAction", {
                { Reflection::ValueType::String },
                { Reflection::ValueType::Boolean },
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    History* history = History::Get();
                    bool succeeded = history->TryBeginAction(inputs[0].AsString());

                    return { succeeded };
                }
            } },

            { "FinishCurrentAction", {
                {},
                {},
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    History* history = History::Get();
                    history->FinishCurrentAction();

                    return {};
                }
            } },

            { "DiscardCurrentAction", {
                {},
                {},
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    History* history = History::Get();
                    history->DiscardCurrentAction();

                    return {};
                }
            } },

            { "CanUndo", {
                {},
                { Reflection::ValueType::Boolean },
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    History* history = History::Get();
                    return { history->CanUndo() };
                }
            } },

            { "CanRedo", {
                {},
                { Reflection::ValueType::Boolean },
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    History* history = History::Get();
                    return { history->CanRedo() };
                }
            } },

            { "Undo", {
                {},
                {},
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    History* history = History::Get();
                    history->Undo();

                    return {};
                }
            } },

            { "Redo", {
                {},
                {},
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    History* history = History::Get();
                    history->Redo();

                    return {};
                }
            } },

            { "GetCurrentActionData", {
                {},
                { REFLECTION_OPTIONAL(Map) },
                [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
                {
                    History* history = History::Get();
                    const std::optional<History::Action>& action = history->GetCurrentAction();

                    if (!action.has_value())
                        return { {} }; // Null

                    Reflection::GenericValue data = dumpActionData(*action);
                    return { data };
                }
            } },

            { "GetActionData", {
                { Reflection::ValueType::Integer },
                { Reflection::ValueType::Map },
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    History* history = History::Get();
                    const History::Action& action = history->GetActionHistory().at(inputs[0].AsInteger());

                    Reflection::GenericValue data = dumpActionData(action);
                    return { data };
                }
            } },
        };

        return methods;
    }
};

static HistoryServiceManager Instance;
