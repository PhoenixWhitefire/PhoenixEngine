-- ed.luau, 14/09/2025
local Editor = {}

local DeveloperTools = game:GetService("DeveloperTools")
local AssetManager = game:GetService("AssetManager")
local ScriptEngine = game:GetService("ScriptEngine")
local PlayerInput = game:GetService("PlayerInput")
local Collections = game:GetService("Collections")
local Interface = game:GetService("Interface")
local Renderer = game:GetService("Renderer")
local History = game:GetService("History")
local Physics = game:GetService("Physics")
local Network = game:GetService("Network")
local Logging = game:GetService("Logging")
local Engine = game:GetService("Engine")

local FALLBACK_IMAGE = "@etextures/editor-icons/fallback.png"
local LOGO_IMAGE = "@etextures/logo.png"

fs.definealias("editres", "./resources")
fs.definealias("etextures", "@editres/textures")
fs.definealias("base", "./resources")

local Configuration = require("./Configuration")

local PluginManager = require("./PluginManager.luau")
local HistoryView = require("./HistoryView.luau")
local TagViewer = require("./TagEditor.luau")
local CoreTools = require("./CoreTools.luau")
local Types = require("./Types.luau")
type Stage = Types.Stage

local CurrentStage: Stage = "None"
local Stages: { [string]: ((number) -> ()) & ((Stage) -> (GameObject & EcDataModel)?) } = {}
local StageUpdateSignal: EventConnection?

local EditorCam = assert(workspace.SceneCamera)
local WorkspaceLink = GameObject.new("TreeLink")
local InterfaceLink = GameObject.new("TreeLink")
local ServicesLink = GameObject.new("TreeLink")
WorkspaceLink.Name = "WorkspaceLink"
InterfaceLink.Name = "InterfaceLink"
ServicesLink.Name = "ServicesLink"

local restoreState = nil

local RecentProjects: { string } = {}
local EditorPreferences = require("./Preferences")

EditorPreferences = EditorPreferences :: { [keyof<typeof(EditorPreferences)>]: index<typeof(EditorPreferences), keyof<typeof(EditorPreferences)>> } & typeof(EditorPreferences)

local GameEditingVM = "GameEditingVM"
local GameVM = "GameVM"

EditorCam.UseSimpleController = true
WorkspaceLink.Parent = workspace
InterfaceLink.Parent = Interface
Physics.Simulating = false
Physics:SetSimulationForcePaused(true)
ScriptEngine:SetVMAllowedExecutionTime(_VMNAME, Configuration.EditorScriptsAllowedExecutionTime)

local ToolOrder = {
    "Explorer",
    "Properties"
}

for _, tool in DeveloperTools:GetToolNames() do
    if not table.find(ToolOrder, tool) then
        table.insert(ToolOrder, tool)
    end
end

local WasHoldingDownM1 = false
local MouseClick = false
local WasHoldingCtrlKeyDown = false
local WasHoldingZKeyDown = false
local WasHoldingYKeyDown = false
local ErrorMessage = ""
local ErrorMessageTimeRemaining = 0
local LastAutosaved = 0
local ViewportRenderPosition = vector.zero
local ViewportRenderSize = vector.zero
local CommandBarHistory: { string } = {}
local CommandBarHistoryCursor = 1

History:EnableRecording()

local savedEditorPreferences = if fs.isfile("./editor/settings.json") then json.parse(assert(fs.read("./editor/settings.json"))) else EditorPreferences
assert(typeof(savedEditorPreferences) == "table")
savedEditorPreferences = savedEditorPreferences :: { [string | number]: JsonSerializable }

for k: keyof<typeof(EditorPreferences)>, v in EditorPreferences do
    if savedEditorPreferences[k] == nil then
        savedEditorPreferences[k] = v
    end
end

EditorPreferences = savedEditorPreferences :: typeof(EditorPreferences)

local commandBarSavedHistory = if fs.isfile("./editor/command-history.json") then json.parse(assert(fs.read("./editor/command-history.json"))) else {}
assert(typeof(commandBarSavedHistory) == "table")
CommandBarHistory = commandBarSavedHistory :: any
CommandBarHistoryCursor = #CommandBarHistory + 1

if not Engine.IsHeadless then
    imgui.stylecolors(if EditorPreferences.UseLightTheme then 'l' else 'd')
end

if not fs.isdirectory("./editor") then
    fs.mkdir("./editor")
    fs.mkdir("./editor/themes")
end

if not fs.isdirectory(EditorPreferences.ProjectsDirectory) then
    fs.mkdir(EditorPreferences.ProjectsDirectory)
end

local ReimportWatchers: { [string]: { Disconnect: () -> (), Models: { GameObject & EcModel } }? } = {}
local ModelModifiedTimes: { [string]: number } = {}

if fs.isfile("./editor/models.json") then
    ModelModifiedTimes = json.parse(assert(fs.read("./editor/models.json"))) :: { [string]: number }
end

local GitIsAvailable = fs.execute("git --version"):find("git version") ~= nil
print(if GitIsAvailable then "Git is available" else "Git is not available")

local function setErrorNotification(Error: string)
    ErrorMessage = Error
    ErrorMessageTimeRemaining = 5
    warn(Error)
end

local function onError(err)
    local errorMessage = `An error occurred: { debug.traceback(err, 2) }`
    if Project.DataModel then
        errorMessage ..= "\n\nThe Editor will perform a save to the file 'resources/scenes/autosave.world', please ensure nothing important is stored at that location before proceeding."
    end

    Logging:Write(errorMessage, Enum.LogType.Error)
    Engine:ShowMessageBox("Editor Error", errorMessage, "ok", "error")

    if Project.DataModel then
        local s, e = AssetManager:SaveScene({ Project.DataModel }, "scenes/autosave.world")
        assert(s, `Failed to save: {e}`)
    end
end

local changeStageTo: (NewStage: Stage) -> ()

local function handleUndoRedoKeybinds()
    local ctrlKeyDown = not imgui.anyitemactive() and PlayerInput:IsKeyPressed(Enum.Key.LeftControl)
    local zKeyDown    = not imgui.anyitemactive() and PlayerInput:IsKeyPressed(Enum.Key.Z)
    local yKeyDown    = not imgui.anyitemactive() and PlayerInput:IsKeyPressed(Enum.Key.Y)

    if ctrlKeyDown and zKeyDown then
        if not WasHoldingCtrlKeyDown or not WasHoldingZKeyDown then
            local success, error = pcall(function()
                History:Undo()
            end)

            if not success then
                assert(error)

                local realError = if typeof(error) == "string" then error:sub(error:find(":") or 1, #error) else tostring(error)
                realError = realError:sub((realError:find(" ") or 0) + 1, #realError)

                setErrorNotification("Undo failed: " .. realError)
            end
        end
    end

    if ctrlKeyDown and yKeyDown then
        if not WasHoldingCtrlKeyDown or not WasHoldingYKeyDown then
            local success, error = pcall(function()
                History:Redo()
            end)

            if not success then
                assert(error)

                local realError = if typeof(error) == "string" then error:sub(error:find(":") or 1, #error) else tostring(error)
                realError = realError:sub((realError:find(" ") or 0) + 1, #realError)

                setErrorNotification("Redo failed: " .. realError)
            end
        end
    end

    WasHoldingCtrlKeyDown = ctrlKeyDown
    WasHoldingZKeyDown = zKeyDown
    WasHoldingYKeyDown = yKeyDown
end

local function editorFrame(DeltaTime)
    if not workspace.SceneCamera then
        workspace.SceneCamera = GameObject.new("Camera")
        workspace.SceneCamera.Parent = workspace
    end

    MouseClick = (not WasHoldingDownM1) and PlayerInput:IsMouseButtonPressed(Enum.MouseButton.Left)

    if not History.IsRecordingAction then
        if CurrentStage == "ProjectEditing" then
            handleUndoRedoKeybinds()
        end
    end

    CoreTools.Update(Project, EditorPreferences, ViewportRenderPosition, ViewportRenderSize, DeltaTime)
    Stages[CurrentStage](DeltaTime)

    WasHoldingDownM1 = PlayerInput:IsMouseButtonPressed(Enum.MouseButton.Left)
end

local function stageUpdate(DeltaTime)
    local s, e = (xpcall::any)(function(dt)
        editorFrame(dt)
    end, onError, DeltaTime)

    if not s then
        warn(`editorFrame error: {e}`)
    end
end

changeStageTo = function(NewStage: Stage)
    assert(Stages[NewStage])

    if StageUpdateSignal then
        StageUpdateSignal:Disconnect()
        StageUpdateSignal = nil
    end

    if Stages[CurrentStage .. "_Leave"] then
        Stages[CurrentStage .. "_Leave"](NewStage)
    end

    local targetDm = game
    if Stages[NewStage .. "_Enter"] then
        targetDm = Stages[NewStage .. "_Enter"](CurrentStage) or targetDm
    end

    PluginManager.OnEditorStageChanged(CurrentStage, NewStage)
    CoreTools.OnEditorStageChanged(NewStage)
    StageUpdateSignal = targetDm.OnFrameBegin:Connect(stageUpdate)

    CurrentStage = NewStage
end

local function saveRecents()
    -- TODO: fix cast
    fs.write(Configuration.RECENT_PROJECTS_PATH, json.encode(RecentProjects :: { JsonSerializable }))
end

local function saveToRecents(Path: string)
    local oldIndex = nil

    for i, v in RecentProjects do
        if v == Path then
            oldIndex = i
            break
        end
    end

    if oldIndex then
        table.remove(RecentProjects, oldIndex)
    end
    table.insert(RecentProjects, Path)
    saveRecents()
end

local function tryToSave(datamodel: GameObject?, firstAttempt: string?)
    if not datamodel then
        Engine:ShowMessageBox("Save failed", "Failed to save: No datamodel", "ok", "error")
        return
    end

    local succeeded, errorMessage

    if not firstAttempt then
        firstAttempt = fs.promptsave()

        if not firstAttempt then
            succeeded = false
            errorMessage = "You did not select a path to start"
        end
    end

    if firstAttempt then
        local modifiedDate, modifiedDateError = (pcall :: (() -> ()) -> (boolean, unknown))(function()
            local conf = json.parse(assert(fs.read("@project/phoenix.conf")))
            assert(typeof(conf) == "table", `config was a { typeof(conf) } and not a table`);

            (conf :: { [string]: JsonSerializable }).Saved = {
                EngineCommit = Engine.CommitHash,
                EngineVersion = Engine.Version,
                EditorBuild = EDITOR_BUILD,
            }
            fs.write("@project/phoenix.conf", json.encode(conf));

            local edconf = json.parse(assert(fs.read("@project/phoenix.editor")))
            assert(typeof(edconf) == "table", `editor config was a { typeof(conf) } and not a table`);

            (edconf :: { [string]: JsonSerializable }).Saved = {
                EngineCommit = Engine.CommitHash,
                EngineVersion = Engine.Version,
                Time = os.time(),
            }
            fs.write("@project/phoenix.editor", json.encode(edconf))
        end)

        if not modifiedDate then
            warn(`Failed to set the Saved date for Project { fs.resolvepath("@project/phoenix.conf") }: { modifiedDateError }`)
        end

        succeeded, errorMessage = AssetManager:SaveScene({ datamodel }, firstAttempt)

        if succeeded then
            return
        end
    end

    local path = firstAttempt

    while true do
        if Engine:ShowMessageBox(
            "Retry?",
            `Failed to save to {if path then fs.resolvepathabsolute(path) else "path"} because of error: { errorMessage }\n\n Select another path and retry?`,
            "yesno",
            "error",
            1
        ) == 0 then
            warn("Gave up trying to save")
            return
        end

        path = fs.promptsave()
        if not path then
            errorMessage = "You did not select a save path"
            continue
        end

        succeeded, errorMessage = AssetManager:SaveScene({ datamodel} , path)
        if succeeded then
            print("Saved")
            break
        end
    end
end

function Editor.SaveProject()
    local editingDatamodel = Project.DataModel

    if CurrentStage == "ProjectPlaying" then
        editingDatamodel = restoreState
    end

    tryToSave(editingDatamodel, "@projres/scenes/root.world")

    if CurrentStage == "ProjectPlaying" then
        if Engine:ShowMessageBox("Also save Playtest state?", "Would you also like to save the state of the Playtest world as a separate file?", "yesno", "question", 1) == 1 then
            tryToSave(Project.DataModel)
        end
    end
end

local openSettingsMenuToConfigureCodeEditor = false

local function openCodeEditor()
    if not EditorPreferences.CodeEditorPath or #EditorPreferences.CodeEditorPath == 0 then
        Engine:ShowMessageBox(
            "Configure Code Editor",
            "You have not configured what Code Editor you would like the Editor to use.\n"
            .. "You can set the command to launch your desired Code Editor in Editor Settings.\n"
            .. "Look up the relevant command for your Code Editor. For example, the command for Visual Studio Code is 'code'."
        )

        openSettingsMenuToConfigureCodeEditor = true
    else
        local result = fs.execute(`{ EditorPreferences.CodeEditorPath } "{ fs.home() }/Documents/Phoenix Projects/{ Project.Name }"`)

        if #result > 1 then
            warn("Open code editor result: ", result)
        end
    end
end

local function stubCoreAssets()
    if not fs.isdirectory("shaders/include") then
        fs.mkdir("shaders/include")
    end

    for _, a in CoreAssets do
        if fs.isfile(a) then
            fs.remove(a)
        end

        fs.write(a, "")
    end
end

local function openProject_(ManualPath: string?)
    local path = ManualPath
    local selecting = not path
    local atLeastOneProject = false

    if selecting then
        imgui.openpopup("Select Project")
    end

    while selecting do
        local open = imgui.beginpopupmodal("Select Project", true)

        if not open then
            print("Closed selector")
            return
        end

        imgui.text("Projects")

        for proj, type in fs.listdir(EditorPreferences.ProjectsDirectory) do
            if type ~= "d" then
                continue
            end

            local conf = fs.read(proj .. "/phoenix.conf")
            local name: string? = nil

            if conf then
                local confdata = json.parse(conf)
                -- TODO: fix cast
                assert(typeof(confdata) == "table")
                local title = confdata["GameTitle" :: string]
                assert(typeof(title) == "string")
                name = title
            end

            if name then
                if imgui.button(name) then
                    selecting = false
                    path = name
                    break
                end

                if not atLeastOneProject then
                    atLeastOneProject = true
                end
            end
        end

        imgui.dummy()

        imgui.endpopup()

        if not atLeastOneProject then
            Engine:ShowMessageBox("No projects", "You do not have any projects")
            return
        end

        game.OnFrameBegin:WaitUntil()
    end

    fs.definealias("project", `{ EditorPreferences.ProjectsDirectory }/{ assert(path) }`)
    fs.definealias("projres", "@project/resources")
    fs.setunqualifiedroot("@project/")
    assert(path)

    if not fs.isfile("@project/phoenix.editor") then
        fs.write("@project/phoenix.editor", "{}")
    end

    local config = json.parse(assert(fs.read("@project/phoenix.conf")))
    assert(typeof(config) == "table", "Configuration file is corrupt")
    config = config :: { [string]: JsonSerializable }

    local objects, error = AssetManager:LoadScene("@projres/scenes/root.world")

    if not objects then
        error = error or "Unknown error"
        Engine:ShowMessageBox("Project Error", `Couldn't open project '{ path }': { error }`, "ok", "error")

        return
    end

    Project.Name = path
    saveToRecents(Project.Name)

    for _, v in objects do
        if v:HasComponent("DataModel") then
            Project.DataModel = v :: any
            break
        end
    end

    if not Project.DataModel then
        Project.DataModel = GameObject.new("DataModel")
        print("Re-created DataModel")
    end

    assert(Project.DataModel)
    Project.DataModel.Name = Project.Name

    -- TODO luau
    Project.Workspace = assert(Project.DataModel :: any):FindChildWithComponent("Workspace")
    if not Project.Workspace then
        local newWorkspace = GameObject.new("Workspace")

        newWorkspace.SceneCamera = GameObject.new("Camera")
        newWorkspace.SceneCamera.Parent = Project.Workspace

        newWorkspace.Name = "Workspace"
        newWorkspace.SceneCamera.Name = "Camera"

        Project.Workspace = newWorkspace;
        warn("Re-created Workspace")
    end

    assert(Project.Workspace)
    if not Project.Workspace.SceneCamera then
        local cam = GameObject.new("Camera")
        cam.Name = "Camera"
        cam.Parent = Project.Workspace
        Project.Workspace.SceneCamera = cam
    end

    EditorCam.Transform = Project.Workspace.SceneCamera.Transform
    WorkspaceLink.Target = Project.Workspace
    InterfaceLink.Target = Project.DataModel:GetService("Interface")
    ServicesLink.Target = Project.DataModel
    ServicesLink.Parent = game

    Project.DataModel:GetService("Environment")

    History:ClearHistory()
    History.TargetDataModel = Project.DataModel

    changeStageTo("ProjectEditing")

    local module = Project.DataModel.LiveScripts
    Project.DataModel.LiveScripts = ""

    Engine:BindDataModel(Project.DataModel)
    ScriptEngine:CreateVM(GameEditingVM)
    Engine:BindDataModel(game)

    ScriptEngine:SetVMAllowedExecutionTime(GameEditingVM, EditorPreferences.ScriptAllowedExecutionTime)

    Project.DataModel.LiveScripts = module
    Project.HasGitRepository = fs.isdirectory("@project/.git")

    if EditorPreferences.AutoLaunchCodeEditorOnProjectOpen then
        openCodeEditor()
    end
end

function Editor.OpenProject(ManualPath: string?)
    PlayerInput.Cursor = Enum.Cursor.NotAllowed

    xpcall(openProject_, function(err)
        Engine:ShowMessageBox("Failed to open project", "Failed to open project: " .. tostring(err), "ok", "error")
    end, ManualPath)

    PlayerInput.Cursor = Enum.Cursor.Arrow
    LastAutosaved = game.Time
end

local function createNewProject_()
    if not fs.isdirectory(EditorPreferences.ProjectsDirectory) then
        fs.mkdir(EditorPreferences.ProjectsDirectory)
    end

    local name = "New Project "
    local nameIndex = 1

    while fs.isdirectory(`{ EditorPreferences.ProjectsDirectory }/{ name .. nameIndex }`) do
        nameIndex += 1

        if nameIndex > 99 then
            error("Tried 99 times to create a project, you need to clean up your Projects folder")
        end
    end

    Project.Name = name .. nameIndex
    Project.IsTempPath = true

    local fullPath = `{ EditorPreferences.ProjectsDirectory }/{ assert(Project.Name) }`
    fs.mkdir(fullPath)

    fs.definealias("project", fullPath)
    fs.definealias("projres", "@project/resources")
    fs.setunqualifiedroot("@project/")

    fs.write("@project/phoenix.conf", json.encode({
        GameTitle = Project.Name,
        ResourcesDirectory = "resources/",
        RootScene = "scenes/root.world",
        Created = {
            Time = os.time(),
            EngineCommit = Engine.CommitHash,
            EngineVersion = Engine.Version,
        },
        Saved = {
            EditorBuild = EDITOR_BUILD,
        },
    }))
    fs.write("@project/phoenix.editor", "{}")

    fs.mkdir("@projres")
    fs.mkdir("@projres/materials")
    fs.mkdir("@projres/scenes")
    fs.mkdir("@projres/sounds")
    fs.mkdir("@projres/shaders")
    fs.mkdir("@projres/shaders/include")
    fs.mkdir("@projres/scripts")
    fs.mkdir("@projres/textures")
    fs.mkdir("@projres/textures/materials")

    fs.mkdir("@projres/scripts/modules")
    fs.write("@projres/scripts/Main.luau", "print(\"Hello, World!\")\n")

    stubCoreAssets()

    for _, v in { "brick", "error", "neon", "plastic", "smoothplastic", "wood", "woodplanks" } do
        fs.copy(`@editres/materials/{v}.mtl`, `@projres/materials/{v}.mtl`)
    end

    fs.copy("@editres/textures/materials", "@projres/textures/materials")
    fs.copy("@editres/textures/Sky1", "@projres/textures/Sky1")
    fs.copy("@editres/textures/image-placeholder.png", "@projres/textures/image-placeholder.png")
    fs.copy("@editres/scripts/defaultconfig.luau", "@projres/scripts/.config.luau")
    fs.copy(LOGO_IMAGE, "@project/icon.png")

    local dm = GameObject.new("DataModel")
    local projWp = GameObject.new("Workspace")
    local camera = GameObject.new("Camera")
    local dl = GameObject.new("DirectionalLight")

    projWp.Name = "Workspace"
    camera.Name = "Camera"
    dl.Name = "DirectionalLight"
    dl.Parent = projWp

    projWp.Parent = dm
    camera.Parent = projWp
    projWp.SceneCamera = camera
    Project.DataModel = dm

    Editor.SaveProject()
    Editor.OpenProject(Project.Name)

    Editor.SavedProjectsCache = nil
end

function Editor.CreateNewProject()
    PlayerInput.Cursor = Enum.Cursor.NotAllowed

    xpcall(createNewProject_, function(err)
        Engine:ShowMessageBox("Failed to create project", "Failed to create new project: " .. tostring(err), "ok", "error")
    end)

    PlayerInput.Cursor = Enum.Cursor.Arrow
end

local TabButtonHovering: string? = nil
local WipTabButtonHovering = TabButtonHovering
local WindowHovering = false

local LIT_COLOR = { 1, 1, 1, 0.05 }
local UNLIT_COLOR = { 1, 1, 1, 0 }

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

    local tint = if Enabled then { 1, 1, 1, 1 } else { 0.5, 0.5, 0.5, 1 }

    if Id == TabButtonHovering then
        imgui.pushstylecolor(5, { 1, 1, 1, 1 })
    end

    local backgroundColor = if EditorPreferences.InvertTabButtonLitColors then LIT_COLOR else UNLIT_COLOR

    if Lit then
        backgroundColor = if EditorPreferences.InvertTabButtonLitColors then UNLIT_COLOR else LIT_COLOR
    end

    local sf = imgui.scalefactor()

    imgui.pushstylecolor(3, backgroundColor)

    imgui.beginchild(Text .. "_Selection", 120 * sf, 150 * sf, "b")

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
        if Enabled and MouseClick then
            return true
        end
    end

    return if Enabled then clicked else false
end

local function whyCannotPlaytest()
    return if History.IsRecordingAction
        then `You can't until the { assert(History.CurrentActionName) } Action finishes`
        else nil
end

local function homeTab(_: number)
    local startY = imgui.cursorposition().y

    local inPlaytest = CurrentStage == "ProjectPlaying"
    local currentActionTooltip = whyCannotPlaytest()

    if inPlaytest then
        if tabButton("Stop", "@etextures/editor-icons/stop.png", not History.IsRecordingAction, nil, nil, currentActionTooltip or "Stop (F5)") then
            changeStageTo("ProjectEditing")
        end
    else
        if tabButton("Start", "@etextures/editor-icons/start.png", not History.IsRecordingAction, nil, nil, currentActionTooltip or "Play (F5)") then
            changeStageTo("ProjectPlaying")
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
                    setErrorNotification(`Failed to import: { importedOrError }`)
                    if importAction then
                        History:DiscardAction(importAction)
                    end

                    return
                end

                assert(Project.Workspace)
                importedOrError:AddTag(REIMPORTABLE_MODEL)
                importedOrError.Parent = Project.Workspace
                DeveloperTools:SetExplorerSelections({ importedOrError })

                local hit = Project.Workspace:Raycast(workspace.SceneCamera.Transform.Position, workspace.SceneCamera.Transform.Forward * 50)

                if hit then
                    importedOrError.Transform = Matrix.translated(hit.Position)
                end

                if importAction then
                    History:FinishAction(importAction)
                end

                ModelModifiedTimes[paths[1]] = fs.lastwritten(paths[1])
                watchModelForReimporting(importedOrError)
            else
                print("No model selected")
            end
        end)
    end

    imgui.sameline()

    if tabButton("Auto re-import\nmodels", "@etextures/editor-icons/auto-reimport.png", true, EditorPreferences.AutoReimportModels) then
        EditorPreferences.AutoReimportModels = not EditorPreferences.AutoReimportModels
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
local didFocusCodeEditorSetting = false

local function updateDevToolsOpenState()
    EditorPreferences.DevToolsOpen = {}

    for _, name in DeveloperTools:GetToolNames() do
        if DeveloperTools:IsToolEnabled(name) then
            table.insert(EditorPreferences.DevToolsOpen, name)
        end
    end
end

local function saveSettings()
    local success, error = pcall(function()
        if CurrentStage == "ProjectEditing" or CurrentStage == "ProjectPlaying" then
            updateDevToolsOpenState()
        end

        fs.write("./editor/settings.json", json.encode(EditorPreferences))

        if openSettingsMenuToConfigureCodeEditor and EditorPreferences.CodeEditorPath and #EditorPreferences.CodeEditorPath > 0 then
            task.defer(openCodeEditor)
        end

        openSettingsMenuToConfigureCodeEditor = false
        didFocusCodeEditorSetting = false

        fs.write("./editor/models.json", json.encode(ModelModifiedTimes :: { [string]: JsonSerializable }))
        fs.write("./editor/command-history.json", json.encode(CommandBarHistory))

        if CurrentStage == "ProjectEditing" or CurrentStage == "ProjectPlaying" then
            ScriptEngine:SetVMAllowedExecutionTime(GameEditingVM, EditorPreferences.ScriptAllowedExecutionTime)

            if CurrentStage == "ProjectPlaying" then
                ScriptEngine:SetVMAllowedExecutionTime(GameVM, EditorPreferences.ScriptAllowedExecutionTime)
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
                saveSettings()
            end
        end

        WasToolEnabled[tool] = enabled

        imgui.sameline()
    end

    local bottomBarOpen = EditorPreferences.ToolsOpen.BottomBar
    if tabButton("Bottom Bar", `@etextures/editor-icons/command-bar.png`, true, bottomBarOpen) then
        EditorPreferences.ToolsOpen.BottomBar = not bottomBarOpen
    end

    imgui.sameline()

    local ouputOpen = EditorPreferences.ToolsOpen.Output
    if tabButton("Output", `@etextures/editor-icons/command-bar.png`, true, ouputOpen) then
        EditorPreferences.ToolsOpen.Output = not ouputOpen
    end

    imgui.dummy()
end

local function pluginsTab(dt: number)
    local sepDepth = 0

    local buttons = PluginManager.GetToolbarButtons(CurrentStage)
    for _, button in buttons do
        if button.IsSeparator then
            imgui.separator() -- goes horizontal for some reason
            imgui.sameline()
            sepDepth += 1
            continue
        end

        imgui.pushid(tostring(sepDepth))

        if tabButton(button.Text, button.Icon or FALLBACK_IMAGE, button.Enabled, button.Lit, button.Text .. sepDepth) then
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

local PrevSelections: { GameObject & EcTransform } = {}

local SelectionPart = GameObject.new("Mesh")
SelectionPart.Material = "boxframe"
SelectionPart.Tint = Color.new(166/255, 200/255, 1)
SelectionPart.PhysicsCollisions = false
SelectionPart.FaceCulling = Enum.FaceCulling.None
SelectionPart.Name = "SelectionPart"

local CommandBarCommand = ""
local CommandBarBlinkFrame = 0
local CommandBarPostRun = false
local IsHoveringViewportTitlebar = false
local IsHoveringViewport = false
local ViewportRight = 0
local ViewportLeft = 0
local ViewportTop = 0
local ViewportBottom = 0
local NewLogMessage = false

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

local Log: { LogMessage } = {}

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

Logging.OnMessaged:Connect(function(Time, Type, Message, ExtraTags, Value)
    if type(Value) == "table" then
        table.freeze(Value :: any)
    end

    if CurrentStage == "ProjectPlaying" and Message:find("Script VM was timed-out") and EditorPreferences.CancelPlaytestWhenScriptTimesOut then
        Engine:ShowMessageBox("Playtest cancelled", "The playtest was cancelled because of a script timing-out.\nYou may disable this behavior in Editor Settings.", "ok", "error")
        task.defer(changeStageTo, "ProjectEditing" :: "ProjectEditing")
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

    local lastLog = Log[#Log]
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

    if #Log >= 999 then
        table.remove(Log, 1)
    end

    table.insert(Log, logMessage)

    NewLogMessage = true
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

local LogMessageHovering: number = -1
local CommandBarActive = false

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
        return ScriptEngine:RunInVM(if CurrentStage == "ProjectEditing" then GameEditingVM else GameVM, CommandBarCommand, "@@projres/scripts/!CommandBar")
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
        if CurrentStage == "ProjectEditing" then
            changeStageTo("ProjectPlaying")
        elseif CurrentStage == "ProjectPlaying" then
            changeStageTo("ProjectEditing")
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

local GameClosedConnection = nil

function Stages.ProjectPlaying_Enter(Stage: Stage)
    PlayerInput.Cursor = Enum.Cursor.NotAllowed

    assert(Project)
    restoreState = Project.DataModel

    assert(Project.DataModel)
    -- TODO luau
    Project.DataModel = assert(Project.DataModel :: any):Duplicate()

    DeveloperTools:SetExplorerRoot(Project.DataModel)
    DeveloperTools:SetExplorerSelections({})
    Physics.Simulating = true

    Project.DataModel.VM = GameVM
    Engine:BindDataModel(Project.DataModel) -- Set the `game` and `workspace` variables to the correct values in game scripts
    ScriptEngine:CreateVM(GameVM)
    ScriptEngine:SetVMAllowedExecutionTime(GameVM, EditorPreferences.ScriptAllowedExecutionTime)

    local projWp = Project.Workspace
    assert(projWp)

    if not projWp.SceneCamera then
        projWp.SceneCamera = GameObject.new("Camera")
        projWp.SceneCamera.Parent = projWp
        projWp.SceneCamera.Name = "Camera"
    end

    workspace.SceneCamera = projWp.SceneCamera

    GameClosedConnection = Project.DataModel.Closing:Connect(function()
        assert(GameClosedConnection)

        GameClosedConnection:Disconnect()
        GameClosedConnection = nil
        changeStageTo("ProjectEditing")
    end)

    Physics:SetSimulationForcePaused(false)

    PlayerInput.Cursor = Enum.Cursor.Arrow
    return Project.DataModel -- Connect to the Project's datamodel `.OnFrameBegin`
end

function Stages.ProjectPlaying(DeltaTime)
    renderMainMenuBar(DeltaTime)
    renderViewport(DeltaTime)
end

function Stages.ProjectPlaying_Leave(DeltaTime)
    assert(restoreState)
    assert(Project.DataModel)

    Engine:BindDataModel(game)

    Project.DataModel:Destroy()
    Project.DataModel = restoreState
    DeveloperTools:SetExplorerRoot(Project.DataModel)

    ScriptEngine:CloseVM(GameVM)
    Physics:SetSimulationForcePaused(true)

    PlayerInput.CursorMode = Enum.CursorMode.Normal
end

local updateInProgress = false
local updaterGitHubToken = ""
local updaterLog = ""
local updaterErr = nil
local switchedToUpdater = 0

function Stages.Update_Enter()
    imgui.openpopup("Updater")
    canUpdate = false
    switchedToUpdater = game.Time
end

local function runUpdate()
    updaterLog = "Update has started.\n"
    updaterErr = nil

    local headers = {
        ["Authorization"] = `Bearer { updaterGitHubToken }`,
        ["User-Agent"] = "PhoenixEngine/EditorUpdate"
    }

    updaterLog ..= "Finding latest workflow run...\n"
    local workflowRunResponse = Network:MakeHttpRequestAsync({
        Url = "https://api.github.com/repos/PhoenixWhitefire/PhoenixEngine/actions/workflows/cmake-multi-platform.yml/runs",
        Headers = headers
    })

    if not workflowRunResponse.Ok then
        error(`{ workflowRunResponse.Status }: { workflowRunResponse.Body }`)
    end

    local workflowRunResponseData = json.parse(workflowRunResponse.Body)
    assert(typeof(workflowRunResponseData) == "table", "Workflow run response data was not a table")
    workflowRunResponseData = workflowRunResponseData :: { [string]: JsonSerializable }

    updaterLog ..= `Total count: { assert(workflowRunResponseData.total_count) }\n`
    updaterLog ..= `Received: { #assert(workflowRunResponseData.workflow_runs :: { JsonSerializable }) }\n`

    local latest = (workflowRunResponseData.workflow_runs :: { { [string]: JsonSerializable } })[1]
    assert(latest)

    updaterLog ..= `Latest title: { assert(latest.display_title) }\n`

    local artifactsUrl = assert(latest.artifacts_url)
    assert(typeof(artifactsUrl) == "string")
    updaterLog ..= `Got artifacts URL: { artifactsUrl }\n`

    local jobsUrl = assert(latest.jobs_url)
    assert(typeof(jobsUrl) == "string")
    updaterLog ..= `Got jobs URL { jobsUrl }\n`

    local jobsResponse = Network:MakeHttpRequestAsync({
        Url = jobsUrl,
        Headers = headers
    })

    if not jobsResponse.Ok then
        error(`{ jobsResponse.Status }: { jobsResponse.Body }`)
    end

    local jobsJson = json.parse(jobsResponse.Body)
    assert(typeof(jobsJson) == "table")

    local jobs = (jobsJson :: { [string]: JsonSerializable }).jobs
    assert(jobs and typeof(jobs) == "table")

    local desiredName = `{ if Engine.TargetPlatform == "Linux" then "ubuntu" else Engine.TargetPlatform:lower() }`
    local foundJob = false

    for _, job in jobs do
        assert(typeof(job) == "table")

        local name =  (job :: { [string]: JsonSerializable }).name
        assert(typeof(name) == "string")

        if name:find(desiredName) and name:find("Release") then
            if (job :: { [string]: JsonSerializable }).conclusion ~= "success" then
                error("The Workflow Job for your platform failed or is still in progress.")
            end

            foundJob = true
            break
        end
    end

    if not foundJob then
        error(`Could not find the Job for your platform ({ desiredName })`)
    end

    updaterLog ..= "Confirmed that the Job for your platform succeeded, proceeding to download artifact\n"

    local artifactsResponse = Network:MakeHttpRequestAsync({
        Url = artifactsUrl,
        Headers = headers
    })

    if not artifactsResponse.Ok then
        error(`{ artifactsResponse.Status }: { artifactsResponse.Body }`)
    end

    local artifactsData = json.parse(artifactsResponse.Body)
    assert(typeof(artifactsData) == "table")
    artifactsData = artifactsData :: { [string]: JsonSerializable }

    updaterLog ..= `Artifact count: { assert(artifactsData.total_count) }\n`
    updaterLog ..= `Checking for { Engine.TargetPlatform } Release artifact...\n`

    assert(typeof(artifactsData.artifacts) == "table")
    local releaseArtifact

    for _, v in artifactsData.artifacts do
        assert(typeof(v) == "table")
        v = v :: { [string]: JsonSerializable }

        local name = v.name
        assert(typeof(name) == "string")

        if name:find(Engine.TargetPlatform) and name:find("Release") then
            releaseArtifact = v
            break
        end
    end

    if not releaseArtifact then
        error(`Could not find { Engine.TargetPlatform }-Release artifact`)
    end

    updaterLog ..= "Found Release artifact\n"
    local bytes = assert(releaseArtifact.size_in_bytes)
    assert(typeof(bytes) == "number")

    updaterLog ..= string.format("Downloading %.2fMB (%i bytes)...\n", bytes / 1000 / 1000, bytes)
    local downloadUrl = assert(releaseArtifact.archive_download_url)
    assert(typeof(downloadUrl) == "string")

    local zipResponse = Network:MakeHttpRequestAsync({
        Url = downloadUrl,
        Headers = headers
    })

    if not zipResponse.Ok then
        error(`{ zipResponse.Status }: { zipResponse.Body }`)
    end

    updaterLog ..= `Got { #zipResponse.Body } bytes\n`
    updaterLog ..= "Writing to update.zip...\n"

    if fs.isfile("./update.zip") then
        error("`update.zip` already exists in the installation directory!")
    end

    fs.write("./update.zip", zipResponse.Body)
    updaterLog ..= "All done!\n"

    task.wait() -- allow the `updaterLog` to be rendered
    Engine:ShowMessageBox("Update", "Close the Editor, extract the 'update.zip' file into the Editor's directory, and you're all done!")
end

function Stages.Update(dt: number)
    Stages.SelectProject(dt)

    if game.Time - switchedToUpdater < 0.1 then
        imgui.setnextwindowsize(750, 500)
        imgui.setnextwindowposition(Renderer.Resolution.x / 2 - 750 / 2, Renderer.Resolution.y / 2 - 500 / 2)
    end

    if imgui.beginpopupmodal("Updater", true) then
        imgui.text("The Updater will download the latest GitHub Actions artifact, which requires a GitHub token.")
        imgui.urllink("Generate a Personal Access Token", "https://github.com/settings/personal-access-tokens")

        imgui.text("GitHub token: ")
        imgui.sameline()

        imgui.setcursorposition(imgui.cursorposition() + vector.create(140, 0))
        if not updateInProgress then
            updaterGitHubToken = imgui.inputstring("##Token", updaterGitHubToken, true)
        else
            imgui.text(string.rep("*", #updaterGitHubToken))
        end

        if not updateInProgress then
            local cxavail = imgui.contentregionavail().x
            local cursor = imgui.cursorposition()
            local btsx = imgui.textsize("Begin").x

            imgui.setcursorposition(getCenteredX(cursor.x, cxavail, btsx), cursor.y)
            if imgui.button("Begin") then
                task.defer(function()
                    local s, e = (pcall :: any)(function()
                        runUpdate()
                    end)

                    if not s then
                        updaterLog ..= "Update failed:\n"
                        updaterErr = e
                    end
                end)

                updateInProgress = true
            end
        end

        if #updaterLog > 0 then
            imgui.separator()
            imgui.text(updaterLog)

            if updaterErr then
                imgui.pushstylecolor(0, { 1, 0, 0, 1 })
                imgui.text(updaterErr)
                imgui.popstylecolor()
            end
        end

        imgui.endpopup()

    elseif game.Time - switchedToUpdater > 0.1 and (not updateInProgress or updaterErr) then
        changeStageTo("SelectProject" :: "SelectProject")

    else
        if game.Time - switchedToUpdater > 0.1 then
            setErrorNotification("You can't do that")
        end

        imgui.openpopup("Updater")
    end
end

function Stages.Update_Leave()
    imgui.openpopup("About")
    canUpdate = true
    updaterErr = nil
    updateInProgress = false
end

local function handleEngineShutdown()
    saveSettings()

    if CurrentStage == "ProjectEditing" or CurrentStage == "ProjectPlaying" then
        local choice = Engine:ShowMessageBox("Save", "Save current Editor file before closing?", "yesno", "warning", 1)

        if choice == 0 then
            warn("Quitting without saving")
        else
            print("Saving before quitting")
            saveProject()
        end

        if Project.DataModel then
            Project.DataModel:Destroy()
        end
    end
end

local function checkIfLatestVersionBuilt()
    local commitHashFetchResult = fs.execute("git rev-parse HEAD")

    -- commit hashes are 40 characters
    if #commitHashFetchResult < 40 then
        return
    end

    if #commitHashFetchResult == 41 and commitHashFetchResult:sub(41, 41) == "\n" then
        -- ends with newline
        commitHashFetchResult = commitHashFetchResult:sub(1, 40)
    end

    if #commitHashFetchResult == 40 and commitHashFetchResult ~= Engine.CommitHash then
        Engine:ShowMessageBox(
            "Version mismatch",
            `Your source files are on commit { commitHashFetchResult:sub(1, 8) }, but the Engine is running on commit { Engine.CommitHash:sub(1, 8) }.\n`
                .. "You may need to re-build the Engine. Behaviour of the Updater may be incorrect."
        )
    end
end

Collections:GetTagAddedSignal(REIMPORTABLE_MODEL):Connect(function(Object)
    if not Object:HasComponent("Model") or not Object:HasComponent("Transform") then
        return
    end

    watchModelForReimporting(Object :: GameObject & EcModel)
end)

Collections:GetTagRemovedSignal(REIMPORTABLE_MODEL):Connect(function(Object)
    if not Object:HasComponent("Model") or not Object:HasComponent("Transform") then
        return
    end

    local model = Object :: GameObject & EcModel
    local watcher = ReimportWatchers[model.ImportPath]

    if not watcher then
        return
    end

    local index = table.find(watcher.Models, model)

    if not index then
        return
    end

    table.remove(watcher.Models, index)

    if #watcher.Models == 0 then
        watcher.Disconnect()
        ReimportWatchers[model.ImportPath] = nil
    end
end)

game:BindToClose(handleEngineShutdown)
task.defer(checkIfLatestVersionBuilt)

if table.find(Engine:GetCliArguments(), "-ed_ci") then
    -- TODO luau cast
    task.defer(changeStageTo, "SelectProject" :: "SelectProject")

    if StageUpdateSignal then
        StageUpdateSignal:Disconnect()
        StageUpdateSignal = nil
    end

    coroutine.wrap(function()
        warn("Running Editor CI checks")

        task.delay(5, function()
            warn("Potential CI stall - Exiting")
            game:Close(1)
        end)

        local success = xpcall(createNewProject_, function(err)
            warn(debug.traceback(err, 1))
        end)

        if not success then
            warn("Failed to create new project!")
            game:Close(1)
            return
        end

        if CurrentStage ~= "ProjectEditing" then
            warn(`Stage is {CurrentStage} and not ProjectEditing`)
            game:Close(1)
            return
        end

        warn("Editor CI checks done")
        CurrentStage = "SelectProject"
        game:Close()
    end)()
else
    changeStageTo("SelectProject")
end

local loadProjectIndex = table.find(Engine:GetCliArguments(), "--project")
if loadProjectIndex then
    local name = Engine:GetCliArguments()[loadProjectIndex + 1]
    if not name then
        Logging:Write("`--project` specified without the project name!", Enum.LogType.Error)
    else
        openProject(name)
    end
end
