// History.hpp, 17/12/2025 - Undo-Redo backend

#include "History.hpp"
#include "GlobalJsonConfig.hpp"

History* History::Get()
{
    static History instance;
    return &instance;
}

void History::RecordEvent(const PropertyEvent& Event)
{
    if (!m_CurrentAction.has_value())
        return;

    m_CurrentAction->Events.push_back(Event);
}

bool History::StartAction(const std::string& Name)
{
    if (!IsRecordingEnabled)
        RAISE_RT("History recording is not enabled!");

    if (m_CurrentAction.has_value())
        return false;

    m_CurrentAction.emplace(Name);
    return true;
}

void History::FinishAction()
{
    if (!m_CurrentAction.has_value())
        RAISE_RT("Called `History::FinishAction` but no action was started!");

    m_ActionHistory.push_back(m_CurrentAction.value());
    m_CurrentAction.reset();

    m_CurrentWaypoint++;
}

bool History::CanUndo()
{
    return m_ActionHistory.size() > 0 && m_CurrentWaypoint != 0 && !m_CurrentAction.has_value();
}

bool History::CanRedo()
{
    return m_ActionHistory.size() > m_CurrentWaypoint && !m_CurrentAction.has_value();
}

void History::Undo()
{
    if (!CanUndo())
    {
        if (m_ActionHistory.size() == 0)
            RAISE_RT("Cannot Undo, no history recorded");
        if (m_CurrentWaypoint == 0)
            RAISE_RT("Cannot Undo, at waypoint 0");
        if (m_CurrentAction.has_value())
            RAISE_RTF("Cannot Undo, action '{}' is not complete", m_CurrentAction->Name);

        RAISE_RT("Cannot Undo right now");
    }

    const Action& lastAction = m_ActionHistory[m_CurrentWaypoint];

    for (size_t i = 0; i < lastAction.Events.size(); i++)
    {
        const PropertyEvent& event = lastAction.Events[i];

        void* p = event.Target.Referred();
        if (!p)
        {
            Log::ErrorF(
                "Cannot Undo event #{} in Action {}: Target of type {} with ID {} is not accessible",
                i, lastAction.Name, s_EntityComponentNames[(size_t)event.Target.Type], event.Target.Id
            );
            continue;
        }

        try
        {
            event.Property->Set(p, event.PreviousValue);
        }
        catch (const std::runtime_error& e)
        {
            Log::ErrorF(
                "Exception thrown while Undoing event #{} in Action {}: {}",
                i, lastAction.Name, e.what()
            );
        }
    }

    m_CurrentWaypoint--;
}

void History::Redo()
{
    if (!CanRedo())
    {
        if (m_ActionHistory.size() <= m_CurrentWaypoint)
            RAISE_RT("Cannot Redo, no history recorded beyond this point");
        if (m_CurrentAction.has_value())
            RAISE_RTF("Cannot Redo, action '{}' is not complete", m_CurrentAction->Name);

        RAISE_RT("Cannot Redo right now");
    }

    const Action& nextAction = m_ActionHistory[m_CurrentWaypoint + 1];

    for (size_t i = 0; i < nextAction.Events.size(); i++)
    {
        const PropertyEvent& event = nextAction.Events[i];

        void* p = event.Target.Referred();
        if (!p)
        {
            Log::ErrorF(
                "Cannot Redo event #{} in Action {}: Target of type {} with ID {} is not accessible",
                i, nextAction.Name, s_EntityComponentNames[(size_t)event.Target.Type], event.Target.Id
            );
            continue;
        }

        try
        {
            event.Property->Set(p, event.NewValue);
        }
        catch (const std::runtime_error& e)
        {
            Log::ErrorF(
                "Exception thrown while Redoing event #{} in Action {}: {}",
                i, nextAction.Name, e.what()
            );
        }
    }

    m_CurrentWaypoint++;
}
