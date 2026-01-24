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

    // `TargetDataModel`/`OwningDataModel` checks are done in `GameObject::SetPropertyValue`
}

void History::ClearHistory()
{
    // We need to do this so that there's a waypoint to go *back* to when we Undo
    m_ActionHistory = { { "<Initial Action>", {} } };

    if (m_CurrentAction.has_value())
        Log::WarningF("`History::ClearHistory` called while Action {} was ongoing", m_CurrentAction->Name);
}

size_t History::TryBeginAction(const std::string& Name)
{
    if (!IsRecordingEnabled)
        RAISE_RT("History recording is not enabled!");

    if (m_CurrentAction.has_value())
    {
        Log::WarningF("`History::TryBeginAction` failed for {} because {} is still in progress", Name, m_CurrentAction->Name);
        return 0;
    }

    m_CurrentAction.emplace(Name);
    return m_CurrentWaypoint + 1;
}

void History::FinishAction(size_t Id)
{
    if (m_CurrentWaypoint + 1 != Id)
        RAISE_RTF("Action ID {} was not valid in `History::FinishAction`", Id);

    if (!m_CurrentAction.has_value())
        RAISE_RT("Called `History::FinishAction` but no action was started!");

    if (m_CurrentWaypoint != m_ActionHistory.size() - 1)
    {
        // TODO multiple timelines
        m_ActionHistory = std::vector<Action>(m_ActionHistory.begin(), m_ActionHistory.begin() + m_CurrentWaypoint + 1);
    }

    m_ActionHistory.push_back(m_CurrentAction.value());
    m_CurrentAction.reset();

    m_CurrentWaypoint++;
}

void History::DiscardAction(size_t Id)
{
    if (m_CurrentWaypoint + 1 != Id)
        RAISE_RTF("Action ID {} was not valid in `History::DiscardAction`", Id);

    if (!m_CurrentAction.has_value())
        RAISE_RT("Called `History::DiscardAction` but no action was started!");

    m_CurrentAction.reset();
}

bool History::CanUndo() const
{
    return m_ActionHistory.size() > 0 && m_CurrentWaypoint != 0 && !m_CurrentAction.has_value();
}

bool History::CanRedo() const
{
    return m_ActionHistory.size() >= m_CurrentWaypoint + 2 && !m_CurrentAction.has_value();
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

    if (m_CurrentWaypoint > m_ActionHistory.size() - 1)
        m_CurrentWaypoint = m_ActionHistory.size() - 1; // TODO not sure why this happens

    const Action& lastAction = m_ActionHistory[m_CurrentWaypoint];

    for (int64_t i = (size_t)lastAction.Events.size() - 1; i >= 0; i--)
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
        if (m_ActionHistory.size() < m_CurrentWaypoint + 2)
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

const std::optional<History::Action>& History::GetCurrentAction() const
{
    return m_CurrentAction;
}

const std::vector<History::Action>& History::GetActionHistory() const
{
    return m_ActionHistory;
}

size_t History::GetCurrentWaypoint() const
{
    return m_CurrentWaypoint;
}
