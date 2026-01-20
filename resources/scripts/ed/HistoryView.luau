-- HistoryView.luau, 17/12/2025
local History: History = game:GetService("History") :: any

local Viewer = {}
Viewer.Open = false

local ActionDataCache = {}
local ActionDataCacheWaypoint = 0

local function renderAction(action, id)
    local open = imgui.treenode(`{action.Name}##{id}`)

    if open then
        for i, e in action.Events do
            imgui.text(`#{i}: Change {e.Target}.{e.Property} from {e.PreviousValue} to {e.NewValue}`)
        end

        imgui.treepop()
    end
end

function Viewer.Render()
    if not Viewer.Open then
        return
    end

    local shouldRender, open = imgui.begin("History Viewer", "", true)

    if not shouldRender then
        imgui.endw()
        return
    end

    Viewer.Open = open

    local currentAction = History:GetCurrentActionData()

    if currentAction then
        imgui.pushstylecolor(0, { 1, 0, 0, 1 })

        renderAction(History:GetCurrentActionData(), -1)

        imgui.popstylecolor()
    end

    for i = History.ActionHistorySize - 1, 0, -1 do
        local data = ActionDataCache[i]
        if not data or ActionDataCacheWaypoint ~= History.CurrentWaypoint then
            data = History:GetActionData(i)
            ActionDataCache[i] = data
            ActionDataCacheWaypoint = History.CurrentWaypoint
        end

        if i == History.CurrentWaypoint then
            imgui.pushstylecolor(0, { 0, 1, 0, 1 })
        end

        renderAction(data, i)

        if i == History.CurrentWaypoint then
            imgui.popstylecolor()
        end
    end

    imgui.endw()
end

return Viewer
