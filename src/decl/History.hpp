// History.hpp, 17/12/2025 - Undo-Redo backend

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "datatype/GameObject.hpp"

class History
{
public:
    static History* Get();

    struct PropertyEvent
    {
        ReflectorRef Target;
        const Reflection::PropertyDescriptor* Property = nullptr;
        Reflection::GenericValue PreviousValue;
        Reflection::GenericValue NewValue;
    };

    void RecordEvent(const PropertyEvent&);

    bool StartAction(const std::string&);
    void FinishAction();

    bool CanUndo();
    bool CanRedo();
    void Undo();
    void Redo();

    bool IsRecordingEnabled = false;

private:
    struct Action
    {
        std::string Name;
        std::vector<PropertyEvent> Events;
    };

    std::optional<Action> m_CurrentAction;
    std::vector<Action> m_ActionHistory;

    size_t m_CurrentWaypoint = 0;
    bool m_IsRecordingEnabled = false;
};
