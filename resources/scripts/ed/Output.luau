-- Output.luau, 29/05/2026
local Output = {
    Log = {} :: { LogMessage }
}

local DeveloperTools = game:GetService("DeveloperTools")
local PlayerInput = game:GetService("PlayerInput")
local Logging = game:GetService("Logging")
local Engine = game:GetService("Engine")

local Preferences = require("@ed/Preferences")
local Theme = require("@ed/Theme")
local State = require("@ed/State")

type LogMessageItem = {
    Content: string,
    DirectValue: unknown,
    Newline: boolean,
    ExtraTags: string,
    CanInteract: boolean,
    Tooltip: string
}
type LogMessage = {
    Closed: boolean,
    Message: { LogMessageItem },
    Type: EnumLogType,
    Time: string,
    Source: string?,
    Line: number?,
    OpenTables: { [unknown]: boolean },
    TablesWithAnyElements: { [unknown]: boolean? },
    TablePrettyOpen: { [string]: boolean },
    TablePrettyChangeOpen: { [string]: boolean }
}

local NewLogMessage = false
local LogMessageHovering = nil

local function canInteractWithLogSubMessage(ExtraTags: string, DirectValue: unknown, source: string?): (boolean, string)
    if typeof(DirectValue) == "GameObject" then
        return true, `Select { tostring(DirectValue) }`
    end

    local shortSrc = ExtraTags:match("TextDocument:.+,")

    if shortSrc and (not source or not source:find("CommandBar")) then
        local lineMatch = ExtraTags:match("DocumentLine:%d+")
        shortSrc = shortSrc:sub(1, assert((shortSrc:find(","))))

        return true, `Open { shortSrc:sub(#("TextDocument:") + 1, #shortSrc - 1) }{ if lineMatch then lineMatch:sub(#("DocumentLine:"), #lineMatch) else "" }`
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

    local shortSrc = ExtraTags:match("TextDocument:.+,")

    if shortSrc then
        local lineMatch = ExtraTags:match("DocumentLine:%d+")
        shortSrc = shortSrc:sub(1, assert((shortSrc:find(","))))

        local line = lineMatch and tonumber(lineMatch:sub(#("DocumentLine:") + 1))
        DeveloperTools:OpenTextDocument(shortSrc:sub(#("TextDocument:") + 1, #shortSrc - 1), line)

        return
    end

    local doc = ExtraTags:match("TextDocument:.+")

    if doc then
        DeveloperTools:OpenTextDocument(doc:sub(#("TextDocument:") + 1, #doc))

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

            local LogColors = {
                [Enum.LogType.None]    = Theme.OutputLogNone,
                [Enum.LogType.Info]    = Theme.OutputLogInfo,
                [Enum.LogType.Warning] = Theme.OutputLogWarning,
                [Enum.LogType.Error]   = Theme.OutputLogError
            }

            local logColor = LogColors[message.Type]
            imgui.pushstylecolor(Enum.ImGuiColor.Text, logColor)
            imgui.pushstylecolor(Enum.ImGuiColor.TextLink, logColor)

            for smi, subMessage in message.Message do
                smid += 1

                if #subMessage.Content == 0 then
                    continue
                end

                local prev = message.Message[smi - 1]
                if not prev or prev.Newline then
                    imgui.sameline()
                end

                local tooltip = subMessage.Tooltip
                local context = message.Message[1].ExtraTags
                if #context <= 2 or context == "LogContext:Main" then
                    context = ""

                    for _, m in message.Message do
                        if #m.ExtraTags > 2 then
                            context = m.ExtraTags
                            tooltip = m.Tooltip
                            break
                        end
                    end
                end

                local _
                _, tooltip = canInteractWithLogSubMessage(context, subMessage.DirectValue)

                if #tooltip == 0 then
                    for _, m in message.Message do
                        if #m.Tooltip > 0 then
                            tooltip = m.Tooltip
                            break
                        end
                    end
                end

                local canInteract = (message.Source or subMessage.CanInteract) and LogMessageHovering == smid and tooltip ~= "From Command Bar"
                local doInteract = false

                if type(subMessage.DirectValue) == "table" then
                    if not message.OpenTables then
                        message.OpenTables = {}
                    end

                    if not message.TablesWithAnyElements then
                        message.TablesWithAnyElements = {}
                    end

                    local function hasAnyElements(t): boolean
                        local cached = message.TablesWithAnyElements[t]

                        if cached == nil then
                            for _ in t do
                                message.TablesWithAnyElements[t] = true
                                return true
                            end
                            message.TablesWithAnyElements[t] = false
                            return false
                        else
                            return cached
                        end
                    end

                    local startX = imgui.cursorposition().x

                    local function renderTable(t: { [unknown]: unknown }, depth: number, pos: vector?)
                        local id = tostring(t)
                        imgui.pushid(id)

                        local open = message.OpenTables[t]
                        local hasElements = hasAnyElements(t)

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
                                imgui.setcursorposition(imgui.cursorposition() + vector.create(startX - 20, 0))

                                local kstr = k
                                if type(k) == "string" then
                                    if not k:match("%W") then
                                        kstr = k
                                    else
                                        kstr = `["{ k }"]`
                                    end
                                else
                                    kstr = `[{ k }]`
                                end

                                if type(v) == "table" then
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
                                    renderTable(v :: any, depth + 1, tpos)
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
                                    else
                                        local vts = tostring(v) .. ","
                                        if type(v) == "string" then
                                            vts = `"{ v }",`
                                        elseif type(v) ~= "number" and type(v) ~= "boolean" then
                                            vts = `[{ v }],`
                                        end

                                        imgui.text(vts)
                                    end
                                end
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

                    imgui.setcursorposition(imgui.cursorposition() - vector.create(8, 0))
                    -- TODO luau cast
                    renderTable(subMessage.DirectValue :: any, 0)
                else
                    if canInteract then
                        doInteract = imgui.textlink(subMessage.Content)
                    else
                        imgui.text(subMessage.Content)
                    end
                end

                if doInteract then
                    interactWithLogSubMessage(context, subMessage.DirectValue)
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

            imgui.popstylecolor()
            imgui.popstylecolor()
        end

        if NewLogMessage then
            imgui.scrollherey()
            NewLogMessage = false
        end

        if not rightClickedLogMessage and imgui.windowhovered() and PlayerInput:IsMouseButtonPressed(Enum.MouseButton.Right) then
            imgui.openpopup("OutputContextMenu")
        end

        if imgui.beginpopup("OutputContextMenu") then
            if imgui.menuitem("Clear Output") then
                table.clear(Output.Log)
            end

            imgui.endpopup()
        end

        LogMessageHovering = logMessageHoveringWip
    end

    if outputBegan then
        imgui.endw()
    end
end

Logging.OnMessaged:Connect(function(Time, Type, Message, ExtraTags, Value)
    if type(Value) == "table" then
        table.freeze(Value :: any)
    end

    if State.CurrentStage == "ProjectPlaying" and Message:find("Script VM was timed-out") and Preferences.CancelPlaytestWhenScriptTimesOut then
        Engine:ShowMessageBox("Playtest cancelled", "The playtest was cancelled because of a script timing-out.\nYou may disable this behavior in Editor Settings.", "ok", "error")
        task.defer(State.ChangeStageTo, "ProjectEditing" :: "ProjectEditing")
    end

    local createsNewline = false

    if Message == " &&" then
        Message = ""
        createsNewline = true
    end

    if #Message >= 2 and Message:sub(-#("&&")) == "&&" then
        Message = Message:sub(1, #Message - 2)
        createsNewline = true
    end

    local source = ExtraTags:match("TextDocument:.+,") or ExtraTags:match("SourceChunkName:.+,")
    if source then
        source = source:sub(#("TextDocument:") + 1, assert((source:find(","))) - 1)
    end

    local line = 1

    if source then
        local lineMatch = ExtraTags:match("DocumentLine:%d+")
        line = lineMatch and tonumber(lineMatch:sub(#("DocumentLine:") + 1))
    end

    local canInteract, tooltip = canInteractWithLogSubMessage(ExtraTags, Value, source)

    local lastLog = Output.Log[#Output.Log]
    if lastLog and not lastLog.Closed then
        if Type == Enum.LogType.None then
            table.insert(lastLog.Message, {
                Content = Message,
                DirectValue = Value,
                ExtraTags = ExtraTags,
                CanInteract = canInteract,
                Tooltip = tooltip,
                Newline = createsNewline
            })

            return
        else
            lastLog.Closed = true
        end
    end

    local firstItem: LogMessageItem = {
        Content = Message,
        DirectValue = Value,
        ExtraTags = ExtraTags,
        CanInteract = canInteract,
        Tooltip = tooltip,
        Newline = createsNewline
    }

    local logMessage: LogMessage = {
        Closed = false,
        Message = { firstItem },
        Type = Type,
        Time = os.date("%H:%M:%S", os.time() - (game.Time - Time)),
        Source = source,
        Line = line,
        OpenTables = {},
        TablesWithAnyElements = {},
        TablePrettyOpen = {},
        TablePrettyChangeOpen = {}
    }

    if #Output.Log >= 999 then
        table.remove(Output.Log, 1)
    end

    table.insert(Output.Log, logMessage)
    NewLogMessage = true
end)

return Output
