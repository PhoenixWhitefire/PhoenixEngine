-- Output.luau, 29/05/2026
local Output = {
    Log = {} :: { LogMessage }
}

local DeveloperTools = game:GetService("DeveloperTools")
local PlayerInput = game:GetService("PlayerInput")
local Logging = game:GetService("Logging")
local Engine = game:GetService("Engine")

local EditorSettingsMenu = require("@ed/EditorSettingsMenu")
local Preferences = require("@ed/Preferences")
local Theme = require("@ed/Theme")
local State = require("@ed/State")

type LogMessageComponent = {
    String: string,
    Value: unknown,
    Metatable: unknown,
    HasExplicitName: boolean,
    CanInteract: boolean,
    Tooltip: string,
}

type LogMessage = {
    Components: { LogMessageComponent },
    Type: EnumLogType,
    ExtraTags: string,
    Time: string,
    Source: string?,
    Line: number?,
    OpenTables: { [unknown]: boolean },
    TablePrettyOpen: { [string]: boolean },
    TablePrettyChangeOpen: { [string]: boolean },
    RepeatCount: number,
    RawContent: string,
}

local NewLogMessage = false
local LogMessageHovering = nil

local renderTable

local function renderTableElement(t, k: unknown, v, message: LogMessage, startX: number, depth: number, more: boolean?, formatName: boolean?)
    formatName = if formatName == nil then true else formatName

    imgui.setcursorposition(imgui.cursorposition() + vector.create(startX - 20, 0))
    local kstr = tostring(k)

    if formatName then
        if typeof(k) == "string" then
            if not k:match("%W") then
                kstr = k
            else
                kstr = `["{ k }"]`
            end
        else
            kstr = `[{ k }]`
        end
    end

    if typeof(v) == "table" then
        local tpos = imgui.cursorposition()
        if imgui.treenode(kstr :: string .. " = ", "dlf | lf") then
            imgui.treepop()
        end

        if imgui.itemclicked() then
            local cid = tostring(v)
            message.TablePrettyChangeOpen[cid] = not message.TablePrettyOpen[cid]
        end

        imgui.sameline()
        imgui.setcursorposition(imgui.cursorposition() - vector.create(12, 0))
        -- TODO luau cast
        renderTable(v :: any, nil, depth + 1, tpos, startX, message)
    else
        if typeof(k) == "GameObject" then
            if imgui.textlink(kstr) then
                DeveloperTools:SetExplorerSelections({ k })
            end
            imgui.sameline()
            imgui.text(" = ")
        else
            if imgui.treenode(kstr :: string .. " = ", "dlf | lf") then
                imgui.treepop()
            end
        end

        imgui.sameline()
        imgui.setcursorposition(imgui.cursorposition() - vector.create(8, 0))

        if typeof(v) == "GameObject" then
            if imgui.textlink(tostring(v)) then
                -- TODO luau cast
                DeveloperTools:SetExplorerSelections({ v :: any })
            end

            if imgui.itemclicked(Enum.MouseButton.Right) then
                imgui.setclipboardtext(tostring(v))
                imgui.settooltip("Copied")
            end

            -- TODO luau cast https://github.com/luau-lang/luau/issues/2627
            if more or next(t :: any, k) then
                imgui.sameline()
                imgui.setcursorposition(imgui.cursorposition() - vector.create(8, 0))
                imgui.text(",")
            end
        else
            local vts = tostring(v)
            if typeof(v) == "string" then
                vts = `"{ v }"`
            elseif typeof(v) ~= "number" and typeof(v) ~= "boolean" then
                vts = `[{ v }]`
            end

            -- TODO luau case https://github.com/luau-lang/luau/issues/2627
            imgui.text(vts .. if more or next(t :: any, k) then "," else "")

            if imgui.itemclicked(Enum.MouseButton.Right) then
                imgui.setclipboardtext(vts)
                imgui.settooltip("Copied")
            end
        end
    end
end

function renderTable(t: { read [unknown]: unknown }, meta: unknown, depth: number, pos: vector?, startX: number, message: LogMessage)
    local id = tostring(t)
    imgui.pushid(id)

    local open = message.OpenTables[t]
    -- TODO luau cast https://github.com/luau-lang/luau/issues/2627
    local hasElements = meta or next(t :: any) ~= nil

    if message.TablePrettyChangeOpen[id] ~= nil then
        imgui.setnextitemopen(message.TablePrettyChangeOpen[id])
        message.TablePrettyChangeOpen[id] = nil
    end

    local prev = imgui.cursorposition()
    local nodeOpen = imgui.treenode("##", "aw | ar | " .. (if depth == 0 then "dlf" else "")  .. (if hasElements then "" else " lf"))
    local after = imgui.cursorposition()

    if imgui.itemclicked() then
        message.TablePrettyChangeOpen[id] = not message.TablePrettyOpen[id]
    end

    imgui.dummy(64, 64)
    imgui.setcursorposition(prev + vector.create(16, 0))
    imgui.text(if (open and hasElements) then "{" else (if hasElements then "{...}" else "{}"))
    imgui.setcursorposition(after)

    message.TablePrettyOpen[id] = nodeOpen

    if nodeOpen then
        message.OpenTables[t] = true

        for k, v in t do
            renderTableElement(t, k, v, message, startX, depth, meta ~= nil)
        end

        if meta then
            renderTableElement(t, "@metatable", meta, message, startX, depth, false, false)
        end

        imgui.treepop()

        if hasElements then
            local p = (if depth > 0 then assert(pos) + vector.create(13, 0) else prev)
            imgui.setcursorposition(vector.create(assert(p.x) + 8, imgui.cursorposition().y))
            imgui.text("}")
        end
    else
        message.OpenTables[t] = false
    end

    imgui.popid()
end

local function canInteractWithLogSubMessage(ExtraTags: string, DirectValue: unknown, source: string?): (boolean, string)
    if typeof(DirectValue) == "GameObject" then
        return true, `Select { tostring(DirectValue) }`
    end

    local shortSrc = ExtraTags:match("Script:.+,")

    if shortSrc and (not source or not source:find("CommandBar")) then
        local lineMatch = ExtraTags:match("Line:%d+")
        shortSrc = shortSrc:sub(1, assert((shortSrc:find(","))))

        return true, `Open { shortSrc:sub(#("Script:") + 1, #shortSrc - 1) }{ if lineMatch then lineMatch:sub(#("Line:"), #lineMatch) else "" }`
    end

    if source and source:find("CommandBar") then
        return false, "From Command Bar"
    end

    return false, ""
end

local function interactWithLogSubMessage(ExtraTags: string, Value: unknown)
    if typeof(Value) == "GameObject" then
        DeveloperTools:SetExplorerSelections({ Value })
        return
    end

    local shortSrc = ExtraTags:match("Script:.+,")

    if shortSrc then
        local lineMatch = ExtraTags:match("Line:%d+")
        shortSrc = shortSrc:sub(1, assert((shortSrc:find(","))))

        local line = lineMatch and tonumber(lineMatch:sub(#("Line:") + 1))
        DeveloperTools:OpenTextDocument(shortSrc:sub(#("Script:") + 1, #shortSrc - 1), line)

        return
    end

    local doc = ExtraTags:match("Script:.+")

    if doc then
        DeveloperTools:OpenTextDocument(doc:sub(#("Script:") + 1, #doc))

        return
    end

    warn(`Failed to figure out the interaction for a log message with tags '{ ExtraTags }'`)
end

function Output.Render()
    local renderOutput = false
    local outputBegan = false
    if Preferences.ToolsOpen.Output then
        renderOutput, Preferences.ToolsOpen.Output = imgui.begin("Output", "h", true)
        outputBegan = true
    end

    if renderOutput then
        local logMessageHoveringWip: number = -1
        local smid = 0
        local rightClickedLogMessage = false

        for mi, message in Output.Log do
            imgui.pushstylecolor(Enum.ImGuiColor.Text, Theme.OutputTimestamp)

            imgui.text(message.Time)

            imgui.popstylecolor()

            local context = message.ExtraTags

            local LogColors = {
                [Enum.LogType.None]    = Theme.OutputLogNone,
                [Enum.LogType.Info]    = Theme.OutputLogInfo,
                [Enum.LogType.Warning] = Theme.OutputLogWarning,
                [Enum.LogType.Error]   = Theme.OutputLogError
            }

            local logColor = LogColors[message.Type]
            imgui.pushstylecolor(Enum.ImGuiColor.Text, logColor)
            imgui.pushstylecolor(Enum.ImGuiColor.TextLink, logColor)

            for smi, component in message.Components do
                smid += 1

                if component.String == "" then
                    continue
                end

                imgui.sameline()

                local tooltip = component.Tooltip or select(2, canInteractWithLogSubMessage(context, component.Value))

                local canInteract = (message.Source or component.CanInteract) and LogMessageHovering == smid and tooltip ~= "From Command Bar"
                local doInteract = false
                local isPrettyPrintableTable = typeof(component.Value) == "table"

                if not isPrettyPrintableTable or component.String:find("table: 0x") ~= 1 then
                    if canInteract then
                        doInteract = imgui.textlink(component.String)
                    else
                        imgui.text(if component.HasExplicitName then component.String else tostring(component.Value))
                    end
                end

                if isPrettyPrintableTable then
                    imgui.sameline()

                    if not message.OpenTables then
                        message.OpenTables = {}
                    end

                    local startX = imgui.cursorposition().x

                    imgui.setcursorposition(imgui.cursorposition() - vector.create(8, 0))
                    -- TODO luau cast
                    renderTable(component.Value :: any, component.Metatable, 0, nil, startX, message)
                end

                if doInteract then
                    interactWithLogSubMessage(context, component.Value)
                end

                if message.Source and message.Line and message.Source ~= "CommandBar" then
                    if imgui.itemclicked(Enum.MouseButton.Right) then
                        DeveloperTools:OpenTextDocument(message.Source, message.Line)
                        rightClickedLogMessage = true
                    end
                end

                if imgui.itemhovered() then
                    logMessageHoveringWip = smid
                end

                if (#tooltip > 0 or #context > 0) and imgui.itemhovered() and imgui.begintooltip() then
                    if #tooltip > 0 then
                        imgui.text(tooltip)
                    end

                    if #context > 0 then
                        if #tooltip > 0 then
                            imgui.newline()
                        end

                        imgui.pushstylecolor(Enum.ImGuiColor.Text, Theme.OutputContextTooltip)

                        imgui.text(context)

                        imgui.popstylecolor()
                    end

                    imgui.endtooltip()
                end
            end

            if message.RepeatCount > 1 then
                imgui.sameline()
                imgui.text(`(x{ message.RepeatCount })`)
            end

            imgui.popstylecolor()
            imgui.popstylecolor()
        end

        if NewLogMessage then
            if imgui.getscrolly() > imgui.getscrollmaxy() - 10 then
                imgui.scrollherey()
            end
            NewLogMessage = false
        end

        if not rightClickedLogMessage and imgui.windowhovered() and PlayerInput:IsMouseButtonPressed(Enum.MouseButton.Right) then
            imgui.openpopup("OutputContextMenu")
        end

        if imgui.beginpopup("OutputContextMenu") then
            if imgui.menuitem("Clear") then
                table.clear(Output.Log)
            end

            if imgui.menuitem("Behavior") then
                EditorSettingsMenu.RequestOpenSection = "Project"
            end

            imgui.endpopup()
        end

        LogMessageHovering = logMessageHoveringWip
    end

    if outputBegan then
        imgui.endw()
    end
end

local function equivalent(a, b)
    if typeof(a) == "table" and typeof(b) == "table" then
        if #a ~= #b then
            return false
        end

        for k, v in a do
            if not equivalent(v, b[k]) then
                return false
            end
        end

        for k, v in b do
            if not equivalent(a[k], v) then
                return false
            end
        end

        return true
    else
        return a == b
    end
end

Logging.OnMessaged:Connect(function(Time, Type, Message, ExtraTags, Values)
    table.freeze(Values)

    local metatables = {}
    local hasExplicitName = {}

    for _, v in Values do
        local val = v.Value

        if typeof(val) == "table" then
            val = val :: { [unknown]: unknown }

            -- TODO luau cast
            for sanitized, value in val do
                if sanitized == "(metatable)" then
                    metatables[v] = value
                    val[sanitized] = nil

                elseif sanitized == "(name)" then
                    v.String = if typeof(value) == "string" then value else v.String
                    hasExplicitName[v] = true
                    val[sanitized] = nil

                elseif typeof(sanitized) == "string" and sanitized:sub(1, 1) == "\\" then
                    val[sanitized] = nil
                    sanitized = sanitized:sub(2, #sanitized)
                    val[sanitized] = value
                end
            end

            table.freeze(v)
        end
    end

    local prev = Output.Log[#Output.Log]

    if prev then
        if #prev.Components == #Values + 1 and prev.Components[1].String == Message then
            local isDuplicate = true

            for i = 2, #prev.Components do
                -- TODO luau cast
                if prev.Components[i].String ~= Values[i - 1].String or not equivalent(Values[i - 1] :: any, prev.Components[i].Value :: any) then
                    isDuplicate = false
                    break
                end
            end

            if isDuplicate then
                prev.RepeatCount += 1
                return
            end
        end
    end

    if State.CurrentStage == "ProjectPlaying" and Message:find("Script VM was timed-out") and Preferences.CancelPlaytestWhenScriptTimesOut then
        Engine:ShowMessageBox("Playtest cancelled", "The playtest was cancelled because of a script timing-out.\nYou may disable this behavior in Editor Settings.", "ok", "error")
        task.defer(State.ChangeStageTo, "ProjectEditing")
    end

    local source = ExtraTags:match("Script:.+,") or ExtraTags:match("SourceChunkName:.+,")
    if source then
        source = source:sub(#("Script:") + 1, assert((source:find(","))) - 1)
    end

    local line = 1

    if source then
        local lineMatch = ExtraTags:match("Line:%d+")
        line = lineMatch and tonumber(lineMatch:sub(#("Line:") + 1))
    end

    local canInteract, tooltip = canInteractWithLogSubMessage(ExtraTags, nil, source)

    local firstItem: LogMessageComponent = {
        String = Message,
        Value = Message,
        HasExplicitName = false,
        CanInteract = canInteract,
        Tooltip = tooltip,
    }

    local components = table.create(#Values + 1, firstItem)

    for i = 1, #Values do
        canInteract, tooltip = canInteractWithLogSubMessage(ExtraTags, Values[i], source)

        table.insert(components, {
            String = Values[i].String,
            Value = Values[i].Value,
            Metatable = metatables[Values[i]],
            HasExplicitName = hasExplicitName[Values[i]] ~= nil,
            CanInteract = canInteract,
            Tooltip = tooltip,
        })
    end

    local logMessage: LogMessage = {
        Components = components,
        Type = Type,
        ExtraTags = ExtraTags,
        Time = os.date("%H:%M:%S", os.time() - (game.Time - Time)),
        Source = source,
        Line = line,
        OpenTables = {},
        TablePrettyOpen = {},
        TablePrettyChangeOpen = {},
        RepeatCount = 1,
        RawContent = Message,
    }

    if #Output.Log >= 999 then
        table.remove(Output.Log, 1)
    end

    table.insert(Output.Log, logMessage)
    NewLogMessage = true
end)

return Output
