-- CommonUI.luau, 30/05/2026
local CommonUI = {}

local DeveloperTools = game:GetService("DeveloperTools")
local ScriptEngine = game:GetService("ScriptEngine")
local AssetManager = game:GetService("AssetManager")
local PlayerInput = game:GetService("PlayerInput")
local Renderer = game:GetService("Renderer")
local Network = game:GetService("Network")
local History = game:GetService("History")
local Logging = game:GetService("Logging")
local Engine = game:GetService("Engine")

local ExternalCodeEditor = require("@ed/ExternalCodeEditor")
local EditorSettingsMenu = require("@ed/EditorSettingsMenu")
local Configuration = require("@ed/Configuration")
local PluginManager = require("@ed/PluginManager")
local ErrorMessage = require("@ed/ErrorMessage")
local HistoryView = require("@ed/HistoryView")
local Preferences = require("@ed/Preferences")
local TagViewer = require("@ed/TagEditor")
local CoreTools = require("@ed/CoreTools")
local Reimport = require("@ed/Reimport")
local Output = require("@ed/Output")
local State = require("@ed/State")
local Theme = require("@ed/Theme")
local Git = require("@ed/Git")

local HoveringOverMenuBar = false
local HoveringOverRibbonTab = false
local RibbonPopupDistance = 16

local TabButtonClicked: string? = nil
local TabButtonHovering: string? = nil
local WipTabButtonHovering = TabButtonHovering
local WindowHovering = false

local IsHoveringViewportTitlebar = false
local IsHoveringViewport = false
local ViewportRight = 0
local ViewportLeft = 0
local ViewportTop = 0
local ViewportBottom = 0

local CommandBarCommand = ""
local CommandBarBlinkFrame = 0
local CommandBarPostRun = false
local CommandBarActive = false
local CommandBarHistory: { string } = {}
local CommandBarHistoryCursor = 1

local commandBarSavedHistory = if fs.isfile("./editor/command-history.json") then json.parse(assert(fs.read("./editor/command-history.json"))) else {}
assert(typeof(commandBarSavedHistory) == "table")
CommandBarHistory = commandBarSavedHistory :: any
CommandBarHistoryCursor = #CommandBarHistory + 1

local PrevSelections: { GameObject & EcTransform } = {}

local ToolOrder = {
    "Explorer",
    "Properties"
}

for _, tool in DeveloperTools:GetToolNames() do
    if not table.find(ToolOrder, tool) then
        table.insert(ToolOrder, tool)
    end
end

local SelectionPart = GameObject.new("Mesh")
SelectionPart.Material = "boxframe"
SelectionPart.Tint = Color.new(166/255, 200/255, 1)
SelectionPart.PhysicsCollisions = false
SelectionPart.FaceCulling = Enum.FaceCulling.None
SelectionPart.Name = "SelectionPart"

local function runCurrentCommand()
    if #CommandBarHistory == 0 or CommandBarHistory[CommandBarHistoryCursor] ~= CommandBarCommand then
        table.insert(CommandBarHistory, CommandBarCommand)

        if #CommandBarHistory > 100 then
            table.remove(CommandBarHistory, 1)
        end

        CommandBarHistoryCursor = #CommandBarHistory
    end

    local commandBarAction = History:TryBeginAction("CommandBarCode")

    if #CommandBarCommand > 1 and CommandBarCommand:sub(1, 1) == "=" then
        CommandBarCommand = `print({ CommandBarCommand:sub(2, #CommandBarCommand) })`
    end

    local callSuccess, runSuccessOrCallError, callError = pcall(function()
        return ScriptEngine:RunInVM(if State.CurrentStage == "ProjectEditing" then Configuration.GameEditingVMName else Configuration.PlaytestVMName, CommandBarCommand, "@@projres/scripts/!CommandBar")
    end)

    if not callSuccess then
        Logging:Write(`Editor experienced an error while running your code: { runSuccessOrCallError }: { callError }`, Enum.LogType.Error)
    end

    if not callSuccess or not runSuccessOrCallError then
        local cmd = CommandBarCommand
        task.wait() -- wait for command bar to be un-focused, then show the command that ran
        CommandBarCommand = cmd
    end

    if commandBarAction then
        History:FinishAction(commandBarAction)
    end

    CommandBarPostRun = true
end

PlayerInput.KeyEvent:Connect(function(kie: KeyInputEvent)
    if kie.Action ~= Enum.InputAction.Pressed then
        return
    end

    if kie.Key == Enum.Key.F5 then
        if State.CurrentStage == "ProjectEditing" then
            State.ChangeStageTo("ProjectPlaying")
        elseif State.CurrentStage == "ProjectPlaying" then
            State.ChangeStageTo("ProjectEditing")
        end
    end

    if not CommandBarActive then
        return
    end


    if kie.Key == Enum.Key.Enter then
        runCurrentCommand()
    elseif #CommandBarHistory > 0 then
        if kie.Key == Enum.Key.Up and CommandBarHistoryCursor > 1 then
            CommandBarHistoryCursor -= 1
            CommandBarCommand = assert(CommandBarHistory[CommandBarHistoryCursor])
            CommandBarBlinkFrame = 1
        end

        if kie.Key == Enum.Key.Down and CommandBarHistoryCursor <= #CommandBarHistory then
            CommandBarHistoryCursor += 1

            if CommandBarHistoryCursor > #CommandBarHistory then
                CommandBarCommand = ""
            else
                CommandBarCommand = assert(CommandBarHistory[CommandBarHistoryCursor])
            end

            CommandBarBlinkFrame = 1
        end
    end
end)

local function isMouseWithinViewport()
    local pos = PlayerInput:GetCursorPosition()

    if pos.x >= ViewportLeft and pos.x <= ViewportRight then
        if pos.y >= ViewportTop and pos.y < ViewportBottom then
            IsHoveringViewport = true
        else
            IsHoveringViewport = false
        end
    else
        IsHoveringViewport = false
    end

    return IsHoveringViewport
end

local function tabButton(Text: string, Icon: string, Enabled: boolean?, Lit: boolean?, Id: string?, Tooltip: string?): boolean
    if Enabled == nil then
        Enabled = true
    end

    if Lit == nil then
        Lit = false
    end

    if not Id then
        Id = Text
    end

    local tint = if Enabled then Theme.TabButtonEnabledTint else Theme.TabButtonDisabledTint

    if Id == TabButtonHovering then
        imgui.pushstylecolor(Enum.ImGuiColor.Border, Theme.TabButtonHoveringBorder)
    end

    local backgroundColor = if Preferences.InvertTabButtonLitColors then Theme.TabButtonLit else Theme.TabButtonUnlit

    if Lit then
        backgroundColor = if Preferences.InvertTabButtonLitColors then Theme.TabButtonUnlit else Theme.TabButtonLit
    end

    local sf = imgui.scalefactor()

    imgui.pushstylecolor(Enum.ImGuiColor.ChildBg, backgroundColor)

    imgui.beginchild(Text .. "_Selection", vector.create(120, 150) * sf, "b")

    local clicked, hovered = false, false
    local cpos = imgui.cursorposition()
    imgui.setcursorposition(cpos.x + 2, cpos.y)

    imgui.image(Icon, vector.create(100, 100) * sf, false, tint, false)
    clicked, hovered = imgui.itemclicked(), imgui.itemhovered()
    imgui.text(Text)
    clicked, hovered = clicked or imgui.itemclicked(), hovered or imgui.itemhovered()

    local windowHovered = imgui.windowhovered()

    imgui.endchild()

    if Enabled and (hovered or windowHovered) then
        WipTabButtonHovering = Id
    end

    if hovered and Tooltip then
        imgui.settooltip(Tooltip)
    end

    imgui.popstylecolor()

    if Id == TabButtonHovering then
        imgui.popstylecolor()

        -- if we are hovering and mouseclick
        if Enabled and TabButtonClicked == Id then
            TabButtonClicked = nil
            return true
        end
    end

    return if Enabled then clicked else false
end

PlayerInput.MouseButtonEvent:Connect(function(mie)
    if mie.Button ~= Enum.MouseButton.Left then
        return
    end

    if mie.Action == Enum.InputAction.Released then
        TabButtonClicked = nil
        return
    end

    TabButtonClicked = TabButtonHovering
end)

local function whyCannotPlaytest()
    return if History.IsRecordingAction
        then `You can't until the { assert(History.CurrentActionName) } Action finishes`
        else nil
end

local function homeTab(_: number)
    local startY = imgui.cursorposition().y

    local inPlaytest = State.CurrentStage == "ProjectPlaying"
    local currentActionTooltip = whyCannotPlaytest()

    if inPlaytest then
        if tabButton("Stop", "@etextures/editor-icons/stop.png", not History.IsRecordingAction, nil, nil, currentActionTooltip or "Stop (F5)") then
            task.defer(State.ChangeStageTo, "ProjectEditing" :: "ProjectEditing")
        end
    else
        if tabButton("Start", "@etextures/editor-icons/start.png", not History.IsRecordingAction, nil, nil, currentActionTooltip or "Play (F5)") then
            task.defer(State.ChangeStageTo, "ProjectPlaying" :: "ProjectPlaying")
        end
    end

    imgui.sameline()

    if tabButton("Import Model", "@etextures/editor-icons/Model.png") then
        task.defer(function()
            local paths = fs.promptopen(nil, { "*.gltf", "*.glb" }, "Models")

            if paths[1] then
                local importAction = History:TryBeginAction("Import Model")
                local success, importedOrError = pcall(function()
                    return AssetManager:ImportModel(paths[1])
                end)

                if not success then
                    ErrorMessage.Push(`Failed to import: { importedOrError }`)
                    if importAction then
                        History:DiscardAction(importAction)
                    end

                    return
                end

                assert(State.Project.Workspace)
                importedOrError:AddTag(Configuration.ReimportableModelTag)
                importedOrError.Parent = State.Project.Workspace
                DeveloperTools:SetExplorerSelections({ importedOrError })

                local hit = State.Project.Workspace:Raycast(workspace.SceneCamera.Transform.Position, workspace.SceneCamera.Transform.Forward * 50)

                if hit then
                    importedOrError.Transform = Matrix.translated(hit.Position)
                end

                if importAction then
                    History:FinishAction(importAction)
                end

                Reimport.ModelModifiedTimes[paths[1]] = fs.lastwritten(paths[1])
                Reimport.WatchModel(importedOrError)
            else
                print("No model selected")
            end
        end)
    end

    imgui.sameline()

    if tabButton("Auto re-import\nmodels", "@etextures/editor-icons/auto-reimport.png", true, Preferences.AutoReimportModels) then
        Preferences.AutoReimportModels = not Preferences.AutoReimportModels
    end

    imgui.sameline()

    CoreTools.RenderControls(tabButton)

    imgui.sameline()
    local cx = imgui.cursorposition().x
    imgui.setcursorposition(cx, startY)

    if tabButton("History", "@etextures/editor-icons/history-viewer.png", true, HistoryView.Open) then
        HistoryView.Open = not HistoryView.Open
    end

    imgui.sameline()
    cx = imgui.cursorposition().x
    imgui.setcursorposition(cx, startY)

    if tabButton("Collections", `@etextures/editor-icons/Folder_{if TagViewer.Open then "Open" else "Closed"}.png`, true, TagViewer.Open) then
        TagViewer.Open = not TagViewer.Open
    end

    imgui.dummy()
end

local WasToolEnabled = {}

local function updateDevToolsOpenState()
    Preferences.DevToolsOpen = {}

    for _, name in DeveloperTools:GetToolNames() do
        if DeveloperTools:IsToolEnabled(name) then
            table.insert(Preferences.DevToolsOpen, name)
        end
    end
end

function CommonUI.SaveSettings()
    local success, error = pcall(function()
        if State.CurrentStage == "ProjectEditing" or State.CurrentStage == "ProjectPlaying" then
            updateDevToolsOpenState()
        end

        fs.write("./editor/settings.json", json.encode(Preferences :: JsonSerializable))

        if State.Project.DataModel and ExternalCodeEditor.RequestOpenSettingsMenuToConfigure and Preferences.CodeEditorPath and #Preferences.CodeEditorPath > 0 then
            task.defer(ExternalCodeEditor.Open, State.Project.Name)
        end

        ExternalCodeEditor.RequestOpenSettingsMenuToConfigure = false

        fs.write("./editor/models.json", json.encode(Reimport.ModelModifiedTimes :: { [string]: JsonSerializable }))
        fs.write("./editor/command-history.json", json.encode(CommandBarHistory :: JsonSerializable))

        if State.CurrentStage == "ProjectEditing" or State.CurrentStage == "ProjectPlaying" then
            ScriptEngine:SetVMAllowedExecutionTime(Configuration.GameEditingVMName, Preferences.ScriptAllowedExecutionTime)

            if State.CurrentStage == "ProjectPlaying" then
                ScriptEngine:SetVMAllowedExecutionTime(Configuration.PlaytestVMName, Preferences.ScriptAllowedExecutionTime)
            end
        end
    end)

    if not success then
        warn(`Failed to save settings: { error }`)
    end
end

local function toolsTab(_: number)
    for _, tool in ToolOrder do
        local enabled = DeveloperTools:IsToolEnabled(tool)

        local iconName = tool
        if iconName == "Renderer" then
            iconName = "Settings"
        end

        if tabButton(tool, `@etextures/editor-icons/{ iconName }.png`, true, enabled) then
            DeveloperTools:SetToolEnabled(tool, not enabled)
        end

        if WasToolEnabled[tool] ~= nil then
            if WasToolEnabled[tool] ~= enabled then
                CommonUI.SaveSettings()
            end
        end

        WasToolEnabled[tool] = enabled

        imgui.sameline()
    end

    local bottomBarOpen = Preferences.ToolsOpen.BottomBar
    if tabButton("Bottom Bar", `@etextures/editor-icons/command-bar.png`, true, bottomBarOpen) then
        Preferences.ToolsOpen.BottomBar = not bottomBarOpen
    end

    imgui.sameline()

    local ouputOpen = Preferences.ToolsOpen.Output
    if tabButton("Output", `@etextures/editor-icons/command-bar.png`, true, ouputOpen) then
        Preferences.ToolsOpen.Output = not ouputOpen
    end

    imgui.dummy()
end

local function pluginsTab(dt: number)
    local sepDepth = 0

    local buttons = PluginManager.GetToolbarButtons(State.CurrentStage)
    for _, button in buttons do
        if button.IsSeparator then
            imgui.separator() -- goes horizontal for some reason
            imgui.sameline()
            sepDepth += 1
            continue
        end

        imgui.pushid(tostring(sepDepth))

        if tabButton(button.Text, button.Icon or Configuration.FallbackImage, button.Enabled, button.Lit, button.Text .. sepDepth) then
            button.OnClick(button)
        end

        imgui.popid()

        imgui.sameline()
    end

    imgui.dummy()
end

local RibbonTabs: { { Name: string, Renderer: (number) -> () } } = {
    {
        Name = "Home",
        Renderer = homeTab
    },
    {
        Name = "Tools",
        Renderer = toolsTab
    },
    {
        Name = "Plugins",
        Renderer = pluginsTab
    }
}

local CurrentRibbonTab: number = 1

local function findEnginePath(): string?
    local commandPath = Engine:GetCliArguments()[1]

    if fs.isfile("./" .. commandPath) then
        return "./" .. commandPath
    end

    if fs.isfile(commandPath) then
        return commandPath
    end

    local choice = Engine:ShowMessageBox("Export", "The Engine executable could not be detected automatically. Please select the Engine executable file.", "okcancel", "warning", 1)
    if choice == 0 then
        return nil
    else
        return fs.promptopen()[1]
    end
end

local function compileScriptBytecodeRecursive(dir)
    for path, ty in fs.listdir(dir) do
        if ty == "f" then
            if path:find(".config.luau") then
                continue
            end

            local extLoc = path:find(".luau")
            if not extLoc or extLoc ~= #path - #(".luau") + 1 then
                continue
            end

            local contents, err = fs.read(path)
            if not contents then
                error(`Error compiling bytecode for '{ path }': { err }`)
            end

            local bytecode = ScriptEngine:CompileToBytecode(
                contents,
                2, -- optimization level
                1  -- debug level
            )
            fs.write(path .. "c", bytecode) -- file extension is `.luauc`

            fs.remove(path) -- remove original source code
        else
            compileScriptBytecodeRecursive(path)
        end
    end
end

local function exportProject()
    State:SaveProject()

    local enginePath = findEnginePath()

    if not enginePath then
        print("Export cancelled")
        return
    end

    imgui.openpopup("Export Project")
    local exportName = State.Project.Name
    local exportDir = fs.promptopenfolder("Choose export directory")
    local exportTo = `{exportDir}/{exportName}`

    if not exportDir then
        print("No export directory selected, export cancelled")
        return
    end

    local keepOnlyScriptBytecode = false
    local prevExportTo = ""

    while true do
        if not imgui.beginpopupmodal("Export Project", true) then
            print("Export cancelled - Popup closed")
            return
        end

        exportName = imgui.inputstring("Export As", exportName)
        if exportName:find("[^%w%s]") then
            imgui.text("Only alphanumeric characters are permitted")
        end

        imgui.text(`Export directory: {exportDir}`)
        imgui.sameline()

        if imgui.button("...") then
            exportDir = fs.promptopenfolder("Change export directory")
        end

        keepOnlyScriptBytecode = imgui.checkbox("Export scripts as bytecode", keepOnlyScriptBytecode)

        exportTo = `{exportDir}/{exportName}`

        imgui.separator()

        imgui.text("Your project will export to:")
        imgui.text(exportTo)

        if prevExportTo ~= exportTo then
            if fs.isdirectory(exportTo) then
                imgui.text("This directory already exists!")
            end
        else
            prevExportTo = exportTo
        end

        if imgui.button("Cancel") then
            print("Export cancelled - Cancel button clicked in Export dialogue")
            imgui.endpopup()
            return
        end

        imgui.sameline()

        local cax = imgui.contentregionavail().x
        local cpos = imgui.cursorposition()
        imgui.setcursorposition(cpos.x + cax - 50, cpos.y)

        if imgui.button("Export") then
            print("Export button clicked")

            if fs.isdirectory(exportTo) then
                local chosenOption = Engine:ShowMessageBox(
                    "Export will overwrite",
                    `The directory:\n\n{ exportTo }\n\nalready exists. Are you sure you want to overwrite it? This will erase all of its content!`,
                    "yesno",
                    "warning"
                )

                if chosenOption == 0 then
                    print("Chose not to overwrite")
                else
                    print("Removing directory to overwrite with export -", exportTo)
                    fs.remove(exportTo)

                    imgui.endpopup()
                    break
                end
            else
                imgui.closecurrentpopup()
                imgui.endpopup()
                break
            end
        end

        imgui.endpopup()
        game.OnFrameBegin:WaitUntil()
    end

    print("Exporting")

    fs.mkdir(exportTo)
    fs.copy("@project", exportTo)
    fs.remove(`{ exportTo }/phoenix.editor`)

    fs.copy(enginePath, `{exportTo}/{State.Project.Name}{if enginePath:find(".exe") then ".exe" else ""}`)

    for _, a in Configuration.CoreAssets do
        local path = `{ exportTo }/resources/{ a }`
        if fs.isfile(path) then
            fs.remove(path)
            fs.copy(`./resources/{ a }`, path)
        end
    end

    if keepOnlyScriptBytecode then
        compileScriptBytecodeRecursive(exportTo .. "/resources/scripts")
    end

    if enginePath:find("x64/Debug") and enginePath:find(".exe") then
        Engine:ShowMessageBox(
            "Windows Debug build",
            "Windows Debug builds cannot be distributed without additional dependencies"
        )
    end

    print("Export done")
end

local function getCenteredX(cursorx: number, cxavail: number, elementWidth: number)
    return cursorx + cxavail / 2 - elementWidth / 2
end

local latestCommit = nil
local canUpdate = false
local isFetchingLatestCommitData = false
local fetchCommitHashData = nil

local function printTable<K, V>(t: { [K]: V }, depth: string, n: string?, printFun: <T>(...T) -> ())
    printFun = printFun or print

    depth = depth or ""

    printFun(depth .. (if n then n .. " =" else ""), tostring(t), "{")
    for k, v in t do
        if typeof(v) ~= "table" then
            printFun(depth .. " ", k, "=", v)
        else
            printTable(v, depth .. "  ", tostring(k), printFun)
        end
    end
    printFun(depth .. "}")
end

local function getLatestGitHubCommit()
    local data = Network:MakeHttpRequestAsync({
        Url = "https://api.github.com/repos/PhoenixWhitefire/PhoenixEngine/commits/main",
        Headers = {
            ["User-Agent"] = "PhoenixEngine/Editor" -- GitHub requires a User Agent for their APIs
        }
    })

    if data.Ok then
        local response = json.parse(data.Body)
        assert(typeof(response) == "table", "response from GitHub API was not a table")

        local commit = (response :: { [string]: JsonSerializable }).sha
        assert(typeof(commit) == "string", ".sha was not a string")

        latestCommit = commit

        if latestCommit ~= Engine.CommitHash then
            canUpdate = true
        end
    else
        -- TODO luau cast
        printTable(data :: any, "", nil, warn)
        fetchCommitHashData = data
    end
end

local function textCentered(text, cursor, cxavail)
    imgui.setcursorposition(getCenteredX(cursor.x, cxavail, imgui.textsize(text).x), cursor.y)
    imgui.text(text)
end

local wasSettingsOpen = false
local OpenGitCommitMenu = false
local GitCommitMessage = ""

local function getGitStatus()
    if Git.Available then
        Git.StatusResult = fs.execute(`git -C "{ fs.resolvepath("@project") }" status`)
        Git.DiffResult = fs.execute(`git -C "{ fs.resolvepath("@project") }" diff`)
    else
        Git.StatusResult = "Git is not available on your system, please ensure it is installed correctly."
    end
end

function CommonUI.RenderMainMenuBar(dt: number)
    if ErrorMessage.Queue[1] then
        local front = ErrorMessage.Queue[1]

        if front.TimeRemaining > 0 then
            imgui.settooltip(front.Message)

            -- after e.g. trying to import a model, the DT will be very high
            if dt < 0.5 then
                front.TimeRemaining -= dt
            end
        else
            table.remove(ErrorMessage.Queue, 1)
        end
    end

    local sf = imgui.scalefactor()
    local pushedCol = State.CurrentStage == "ProjectPlaying"

    if pushedCol then
        imgui.pushstylecolor(Enum.ImGuiColor.MenuBarBg, Theme.PlaytestMenubarColor)
    end

    local render = imgui.beginmainmenubar()

    if pushedCol then
        imgui.popstylecolor()
    end

    if not render then
        return
    end

    local mainMenuCursorY = PlayerInput:GetCursorPosition().y
    HoveringOverMenuBar = mainMenuCursorY < RibbonPopupDistance and PlayerInput.CursorMode == Enum.CursorMode.Normal

    local openAboutWindow = false
    local openSettingsWindow = false

    if imgui.beginmenu("File") then
        if State.CurrentStage == "ProjectEditing" then
            if imgui.menuitem("Undo", History.CanUndo) then
                History:Undo()
            end
            if not History.CanUndo then
                imgui.setitemtooltip(History:GetCannotUndoReason())
            end

            if imgui.menuitem("Redo", History.CanRedo) then
                History:Redo()
            end
            if not History.CanRedo then
                imgui.setitemtooltip(History:GetCannotRedoReason())
            end

            if imgui.menuitem("Open in Code Editor") then
                task.defer(ExternalCodeEditor.Open, State.Project.Name)
            end

            if imgui.menuitem("Open Project folder") then
                fs.systemopen("@project")
            end
        end

        if imgui.menuitem("Save", State.CurrentStage == "ProjectEditing") then
            State:SaveProject()
        end

        if State.CurrentStage == "ProjectEditing" then
            if imgui.menuitem("Project Settings") then
                local conf = json.parse(assert(fs.read("@project/phoenix.conf"))) :: any

                task.defer(function()
                    imgui.openpopup("Project Settings")

                    local newProjectName = State.Project.Name
                    imgui.setnextwindowsize(600, 300)

                    while true do
                        if not imgui.beginpopupmodal("Project Settings", true) then
                            break
                        end

                        newProjectName = imgui.inputstring("Project Name", newProjectName)

                        if newProjectName:match("[^%w%s]") then
                            imgui.text("Only a-z, A-Z, 0-9 (alphanumeric) is allowed")
                        end

                        if newProjectName ~= State.Project.Name and imgui.button("Save new name") then
                            local buttonSelection = Engine:ShowMessageBox("Rename project?", "Are you sure you want to rename the project?", "yesno")

                            if buttonSelection == 1 then
                                local prevName = State.Project.Name

                                fs.rename(`{ Preferences.ProjectsDirectory }/{ State.Project.Name }`, newProjectName)
                                fs.definealias("project", `{ Preferences.ProjectsDirectory }/{ newProjectName }`)

                                State.Project.Name = newProjectName

                                local conf = json.parse(assert(fs.read("@project/phoenix.conf")))
                                assert(typeof(conf) == "table")
                                -- TODO: fix cast
                                conf["GameTitle" :: string] = newProjectName

                                fs.write(
                                    "@project/phoenix.conf",
                                    json.encode(conf)
                                )

                                table.remove(State.RecentProjects, table.find(State.RecentProjects, prevName))
                                State:SaveToRecents(newProjectName)
                            else
                                newProjectName = State.Project.Name
                            end
                        end

                        imgui.text("Project icon:")

                        if imgui.imagebutton("Icon", "@project/icon.png", vector.create(150, 150)) then
                            local buttonSelection = Engine:ShowMessageBox(
                                "Change icon?",
                                "Change the project icon? Note that the original icon file will be overwritten!",
                                "yesno",
                                "question"
                            )

                            if buttonSelection == 1 then
                                task.defer(function()
                                    local path = fs.promptopen(nil, { "*.png" , "*.jpg", "*.jpeg" }, "Images")[1]

                                    if path then
                                        fs.remove("@project/icon.png")
                                        fs.copy(path, "@project/icon.png")
                                        AssetManager:UnloadTexture("@project/icon.png") -- force the texture to be reloaded
                                    else
                                        print("No icon selected")
                                    end
                                end)
                            end
                        end

                        conf.DefaultVMAllowedExecutionTime = imgui.inputnumber("Exported script allowed execution time", conf.DefaultVMAllowedExecutionTime or 30)

                        if imgui.button("Save") then
                            fs.write("@project/phoenix.conf", json.encode(conf))
                        end

                        imgui.endpopup()

                        game.OnFrameBegin:WaitUntil()
                    end
                end)
            end

            if Preferences.GitIntegration then
                if imgui.beginmenu("Git", Git.Available) then
                    if not State.Project.HasGitRepository then
                        if imgui.menuitem("Initialize repository") then
                            task.defer(function()
                                print(fs.execute(`git -C "{ fs.resolvepath("@project") }" init`))
                                State.Project.HasGitRepository = fs.isdirectory("@project/.git")

                                if State.Project.HasGitRepository then
                                    fs.write("@project/.gitignore", "# Generated by Phoenix Engine Editor\n\nphoenix.editor\n")
                                end
                            end)
                        end
                    else
                        if imgui.menuitem("Commit") then
                            task.defer(function()
                                State:SaveProject()
                                getGitStatus()
                                OpenGitCommitMenu = true
                            end)
                        end
                    end

                    imgui.endmenu()
                end

                if not Git.Available then
                    imgui.setitemtooltip("Git was not detected on your system, please ensure it is installed correctly")
                end
            end

            if imgui.menuitem("Export") then
                task.defer(exportProject)
            end

            if imgui.menuitem("Close") then
                -- TODO luau cast
                task.defer(State.ChangeStageTo, "SelectProject" :: "SelectProject")
            end
        end

        if State.CurrentStage == "SelectProject" then
            if imgui.menuitem("Open") then
                State.SavedProjectsCache = nil
                task.defer(State.OpenProject, State, nil) -- TODO luau type error when `nil` is not supplied
            end

            if imgui.menuitem("New") then
                task.defer(State.CreateNewProject, State, nil)
            end
        end

        if imgui.menuitem("Editor Settings") then
            openSettingsWindow = true
        end

        if imgui.menuitem("About") then
            openAboutWindow = true
        end

        imgui.endmenu()
    end

    if OpenGitCommitMenu then
        imgui.openpopup("Commit")
        OpenGitCommitMenu = false
        GitCommitMessage = ""
    end

    if imgui.beginpopupmodal("Commit", true) then
        imgui.text(Git.StatusResult)
        imgui.separator()

        if Git.StatusResult:find("nothing to commit") then
            imgui.text("No changes to commit.")
        else
            local diffOpen = imgui.treenode("Diff", "dlf | fw")

            if diffOpen then
                imgui.text(Git.DiffResult)
                imgui.treepop()
            end

            --local tsy = imgui.textsize("").y

            local cavail = imgui.contentregionavail()
            local cxavail = cavail.x
            local cursor = imgui.cursorposition()

            --imgui.setcursorposition(vector.create(cursor.x, cavail.y + tsy * 4 + 20))
            --imgui.dummy()

            imgui.text("All files will be committed automatically.\nCommit message:")
            imgui.setnextitemwidth(cxavail)
            GitCommitMessage = imgui.inputstring("##CommitMessage", GitCommitMessage)

            if #GitCommitMessage == 0 then
                imgui.text("Please enter a commit message.")
            end

            cursor = imgui.cursorposition()
            local buttonText = "Commit"
            imgui.setcursorposition(getCenteredX(cursor.x, cxavail, imgui.textsize(buttonText).x), cursor.y + 10 * sf)

            if imgui.button(buttonText) then
                task.defer(function()
                    GitCommitMessage = string.gsub(GitCommitMessage, '"', '\\"')
                    print(fs.execute(`git -C "{ fs.resolvepath("@project") }" add .`))
                    print(fs.execute(`git -C "{ fs.resolvepath("@project") }" commit -m "{ GitCommitMessage }" -a`))
                    getGitStatus()
                end)
            end
        end

        imgui.endpopup()
    end

    if State.CurrentStage ~= "SelectProject" then
        imgui.separator()

        WipTabButtonHovering = nil
        WindowHovering = false

        for i, tab in RibbonTabs do
            if imgui.menuitem(if CurrentRibbonTab == i then `[{tab.Name}]` else tab.Name) then
                CurrentRibbonTab = i
            end
        end

        if (HoveringOverMenuBar or HoveringOverRibbonTab or TabButtonHovering ~= nil or WindowHovering)
            and not PlayerInput:IsMouseButtonPressed(Enum.MouseButton.Right)
        then
            local windowWidth = Renderer.Resolution.x

            local TAB_HEIGHT = 175 * sf

            imgui.setnextwindowposition(0, 16 * sf)
            imgui.setnextwindowsize(windowWidth, TAB_HEIGHT)

            imgui.begin("RibbonTabContents", "nt | nr | nm | nc | ns | nd | h")
            HoveringOverRibbonTab = imgui.windowhovered() or imgui.anyitemactive()
            RibbonTabs[CurrentRibbonTab].Renderer(dt)
            imgui.endw()

            RibbonPopupDistance = TAB_HEIGHT -- fixes ribbon closing when enabling tools
        else
            RibbonPopupDistance = 16 * sf
        end

        TabButtonHovering = WipTabButtonHovering
    end

    imgui.endmainmenubar()

    if openAboutWindow then
        imgui.openpopup("About")

        local ws = Renderer.Resolution

        local ABOUT_SIZE_X, ABOUT_SIZE_Y = 375 * sf, 340 * sf

        imgui.setnextwindowposition((ws.x - ABOUT_SIZE_X) / 2, (ws.y - ABOUT_SIZE_Y) / 2);
        imgui.setnextwindowsize(ABOUT_SIZE_X, ABOUT_SIZE_Y);
    end

    if imgui.beginpopupmodal("About", true) then
        local cxavail = imgui.contentregionavail().x
        local cursor = imgui.cursorposition()

        local LOGO_SIZE = 150 * sf

        imgui.setcursorposition(getCenteredX(cursor.x, cxavail, LOGO_SIZE), cursor.y)
        imgui.image(Configuration.GetLogo("EditorSettings"), vector.create(LOGO_SIZE, LOGO_SIZE))

        cursor += vector.create(0, 160 * sf)

        local tts = imgui.textsize("")

        textCentered("Phoenix Engine Editor", cursor, cxavail)
        cursor += vector.create(0, tts.y + 2 * sf)

        textCentered(Engine.Version, cursor, cxavail)
        cursor += vector.create(0, tts.y + 15 * sf)

        imgui.setcursorposition(getCenteredX(cursor.x, cxavail, imgui.textsize(Configuration.GitHubUrl).x), cursor.y)
        imgui.urllink(Configuration.GitHubUrl)
        cursor += vector.create(0, tts.y + 2 * sf)

        local commitText = `{ Engine.CommitTag } ({ Engine.CommitHash:sub(1, 8) })`

        imgui.setcursorposition(getCenteredX(cursor.x, cxavail, imgui.textsize(commitText).x), cursor.y)
        imgui.urllink(commitText, `{ Configuration.GitHubUrl }/commit/{ Engine.CommitHash }`)
        cursor += vector.create(0, tts.y + 16 * sf)

        if not isFetchingLatestCommitData and not latestCommit then
            isFetchingLatestCommitData = true
            task.defer(getLatestGitHubCommit)
        end

        if latestCommit then
            if latestCommit ~= Engine.CommitHash then
                textCentered("There is a newer version available.", cursor, cxavail)
                cursor += vector.create(0, tts.y + 2 * sf)

                textCentered(`Latest: { latestCommit:sub(1, 8) }`, cursor, cxavail)
                cursor += vector.create(0, tts.y + 4 * sf)

                local buttonText = "Download"
                imgui.setcursorposition(getCenteredX(cursor.x, cxavail, imgui.textsize(buttonText).x), cursor.y)

                -- TODO
                canUpdate = false

                if canUpdate and imgui.button(buttonText) then
                    if State.CurrentStage ~= "SelectProject" then
                        Engine:ShowMessageBox("Update", "You must close the current project to update")
                    elseif fs.isfile("./update.zip") then
                        Engine:ShowMessageBox("Update", "Delete or rename the 'update.zip' file in the Editor's installation directory to proceed", "ok", "error")
                    else
                        imgui.closecurrentpopup()
                        --State:ChangeStageTo("Update")
                        Engine:ShowMessageBox("W.I.P.", "Not in usable state after refactor :>")
                    end
                else
                    imgui.dummy()
                end
            else
                local urutdText = "You are on the latest available version."
                local urutdtsx = imgui.textsize(urutdText).x

                imgui.setcursorposition(getCenteredX(cursor.x, cxavail, urutdtsx), cursor.y + 32 * sf)
                imgui.text(urutdText)
            end
        elseif fetchCommitHashData then
            textCentered("Failed to check for updates.", cursor, cxavail)
            cursor += vector.create(0, tts.y + 2 * sf)
            textCentered(`Status: { fetchCommitHashData.Status }`, cursor, cxavail)
            cursor += vector.create(0, tts.y + 2 * sf)
            textCentered(`Error: { fetchCommitHashData.Body }`, cursor, cxavail)
            imgui.setitemtooltip(fetchCommitHashData.Body)
        end

        imgui.endpopup()
    end

    if (openSettingsWindow or EditorSettingsMenu.RequestOpenSection) and not wasSettingsOpen then
        imgui.openpopup("Editor Settings")

        local SETTINGS_SIZE_X, SETTINGS_SIZE_Y = 1000 * sf, 600 * sf

        local ws = Renderer.Resolution

        imgui.setnextwindowposition((ws.x - SETTINGS_SIZE_X) / 2, (ws.y - SETTINGS_SIZE_Y) / 2)
        imgui.setnextwindowsize(SETTINGS_SIZE_X, SETTINGS_SIZE_Y)
    end

    if imgui.beginpopupmodal("Editor Settings", true) then
        EditorSettingsMenu.Render()

        imgui.endpopup()
        wasSettingsOpen = true

    elseif wasSettingsOpen then
        wasSettingsOpen = false
        CommonUI.SaveSettings()
    end
end

function CommonUI.RenderViewport(DeltaTime: number)
    if State.CurrentStage == "ProjectEditing" and State.Project.Workspace then
        local mouseVec = workspace:ScreenPointToVector(PlayerInput:GetCursorPosition(), 1000)
        local rayresult = if not PlayerInput.IsMouseSunk
                            then State.Project.Workspace:Raycast(workspace.SceneCamera.Transform.Position, mouseVec, { SelectionPart })
                            else nil

        if rayresult then
            SelectionPart.Transform = rayresult.Object.Transform
            SelectionPart.Size = rayresult.Object.Size + vector.create(0.05, 0.05, 0.05)
            SelectionPart.Parent = workspace

            if PlayerInput:IsMouseButtonPressed(Enum.MouseButton.Left) and isMouseWithinViewport() then
                if PlayerInput:IsKeyPressed(Enum.Key.LeftControl) then -- left control
                    local sels = DeveloperTools:GetExplorerSelections()

                    if not table.find(sels, rayresult.Object) then
                        table.insert(sels, rayresult.Object :: GameObject)
                    end

                    DeveloperTools:SetExplorerSelections(sels)
                else
                    DeveloperTools:SetExplorerSelections({ rayresult.Object })
                end
            end
        else
            SelectionPart.Parent = nil
        end
    else
        for _, v in PrevSelections do
            v:Destroy()
        end
        PrevSelections = {}
    end

    local newSelections = DeveloperTools:GetExplorerSelections()
    local selmap: { [number]: GameObject } = {}
    for i, sel_ in newSelections do
        if not sel_:HasComponent("Transform") then
            continue
        end

        local sel = (sel_ :: any) :: GameObject & EcTransform
        selmap[sel.ObjectId] = sel

        if not PrevSelections[sel.ObjectId] then
            local new = SelectionPart:Duplicate()
            -- TODO luau report bug cast
            new = (new :: any)
            new.Transform, new.Size = sel.Transform, sel.Size + vector.create(0.1, 0.1, 0.1)
            new.Tint = Color.new(84/255, 150/255, 1)
            new.Parent = workspace
            PrevSelections[sel.ObjectId] = new
        else
            local box = PrevSelections[sel.ObjectId]
            box.Transform, box.Size = sel.Transform, sel.Size
        end
    end

    for id, box in PrevSelections do
        local sel = selmap[id]
        if not sel or not sel:HasComponent("Transform") then
            box:Destroy()
            PrevSelections[id] = nil
        end
    end

    PluginManager.OnUpdate(DeltaTime, State.CurrentStage)
    CoreTools.Update(State.Project, Preferences, State.ViewportRenderPosition, State.ViewportRenderSize, DeltaTime)
    HistoryView.Render()
    TagViewer.Render()

    if Preferences.ToolsOpen.BottomBar then
        local w = Renderer.Resolution

        imgui.setnextwindowposition(0, w.y - 35)
        imgui.setnextwindowsize(w.x, 35 * imgui.scalefactor())

        imgui.begin("Bottom Bar", "nt | nd | nr | ns")

        imgui.setcursorposition(imgui.cursorposition() + vector.create(0, 2))
        imgui.text("> ")
        imgui.sameline()
        imgui.setcursorposition(imgui.cursorposition() - vector.create(0, 2))

        CommandBarCommand = imgui.inputstring(if CommandBarBlinkFrame == 1 then "##CommandBarBlinking" else "##Command", CommandBarCommand)
        CommandBarActive = imgui.itemactive()

        if CommandBarBlinkFrame == 1 then
            CommandBarBlinkFrame += 1

        elseif CommandBarBlinkFrame == 2 then
            CommandBarBlinkFrame = 0
            imgui.focuskeyboardhere(-1)
        end

        if CommandBarPostRun then
            imgui.focuskeyboardhere(-1)
            CommandBarPostRun = false
        end

        imgui.sameline()

        local noteStr = `{ Engine.Framerate } FPS ({ State.CurrentStage }) | { State.Project.Name }`
        imgui.text(noteStr)
        imgui.setitemtooltip(noteStr)

        imgui.endw()
    end

    Output.Render()

    local canInteract = IsHoveringViewportTitlebar --or (not IsHoveringViewport and imgui.windowhovered("a | b"))
    local render, open = imgui.begin(State.Project.Name .. "###Viewport", if canInteract then "nb" else "ni | nb", true)

    if not open then
        task.defer(State.ChangeStageTo, "SelectProject" :: "SelectProject")
    end

    if not render then
        PlayerInput:SetViewportInputRect(vector.zero, vector.create(1, 1))
        imgui.endw()
        return
    end

    local cpos = PlayerInput:GetCursorPosition()
    local w = imgui.windowposition()
    local ws = imgui.windowsize()

    local ews = Renderer.Resolution
    State.ViewportRenderPosition = vector.create(w.x, w.y)
    State.ViewportRenderSize = vector.create(ws.x, ws.y)

    imgui.setviewportdockspace(0, 19, ews.x, ews.y - (if Preferences.ToolsOpen.BottomBar then 54 else 19))
    PlayerInput:SetViewportInputRect(State.ViewportRenderPosition, State.ViewportRenderSize)

    ViewportLeft = w.x - 15
    ViewportRight = w.x + ws.x + 15
    ViewportTop = w.y
    ViewportBottom = w.y + ws.y

    if cpos.x >= w.x - 15 and cpos.x <= w.x + ws.x + 15 then
        if cpos.y >= w.y and cpos.y < w.y + 16 then
            IsHoveringViewportTitlebar = true
        else
            IsHoveringViewportTitlebar = false
        end
    else
        IsHoveringViewportTitlebar = false
    end
    isMouseWithinViewport()

    imgui.setcursorposition(0, ws.y - ews.y)

    imgui.image("!Framebuffer:Main", nil, true)

    imgui.endw()
end

CommonUI.GetCenteredX = getCenteredX
return CommonUI
