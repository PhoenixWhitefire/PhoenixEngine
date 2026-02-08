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
        ObjectRef TargetObject;
        const Reflection::PropertyDescriptor* Property = nullptr;
        Reflection::GenericValue PreviousValue;
        Reflection::GenericValue NewValue;
    };

    void RecordEvent(const PropertyEvent&);
    void ClearHistory();

    std::optional<size_t> TryBeginAction(const std::string&);
    void FinishAction(size_t);
    void DiscardAction(size_t);

    std::string GetCannotUndoReason() const;
    std::string GetCannotRedoReason() const;

    bool CanUndo() const;
    bool CanRedo() const;
    void Undo();
    void Redo();

    uint32_t TargetDataModel = PHX_GAMEOBJECT_NULL_ID;
    bool IsRecordingEnabled = false;

    struct Action
    {
        std::string Name;
        std::vector<PropertyEvent> Events;
        size_t Id = 0;
    };

    const std::optional<Action>& GetCurrentAction() const;
    const std::vector<Action>& GetActionHistory() const;
    size_t GetCurrentWaypoint() const;

private:
    std::optional<Action> m_CurrentAction;
    std::vector<Action> m_ActionHistory = { { "<Initial Action>", {} } };

    size_t m_CurrentWaypoint = 0;
};
