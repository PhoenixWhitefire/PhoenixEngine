-- HistoryView.luau, 17/12/2025
local History = game:GetService("History")

local Viewer = {}
Viewer.Open = false

local ActionDataCache = {}
local ActionDataCacheWaypoint = 0
local ContextMenuTargetId = 0

local function renderAction(action: HistoryAction, id)
    local openContextMenu = false

    local open = imgui.treenode(`{action.Name}##{id}`)

    if imgui.itemclicked(Enum.MouseButton.Right) then
        openContextMenu = true
        ContextMenuTargetId = id
    end

    if open then
        for i, e in action.Events do
            local str

            if e.Type == "PropertyChanged" then
                str = `#{i}: Change {e.Target}.{e.Property} from {e.PreviousValue} to {e.NewValue}`
            else
                str = `#{i}: { e.Action } { if e.Type == "Component" then e.Component else e.Tag } { e.Type:lower() } { if e.Action == "Add" then "to" else "from" } { e.Target }`
            end

            assert(str)
            imgui.text(str)
        end

        imgui.treepop()
    end

    if openContextMenu then
        imgui.openpopup("ActionContextMenu")
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

        renderAction(currentAction, -1)

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

    if imgui.beginpopup("ActionContextMenu") then
        if imgui.menuitem("Go to here") then
            local dir = math.sign(ContextMenuTargetId - History.CurrentWaypoint)

            if dir ~= 0 then
                for _ = History.CurrentWaypoint, ContextMenuTargetId, dir do
                    if History.CurrentWaypoint == ContextMenuTargetId then
                        break
                    end

                    if dir == -1 then
                        if History.CanUndo then
                            History:Undo()
                        else
                            warn(`'Go to here' failed because { History:GetCannotUndoReason() }`)
                            break
                        end
                    else
                        if History.CanRedo then
                            History:Redo()
                        else
                            warn(`'Go to here' failed because { History:GetCannotRedoReason() }`)
                            break
                        end
                    end
                end
            end
        end

        imgui.endpopup()
    end

    imgui.endw()
end

return Viewer
