-- State.luau, 30/05/2026
local ScriptEngine = game:GetService("ScriptEngine")
local AssetManager = game:GetService("AssetManager")
local PlayerInput = game:GetService("PlayerInput")
local Workspace = game:GetService("Workspace")
local Interface = game:GetService("Interface")
local History = game:GetService("History")
local Physics = game:GetService("Physics")
local Engine = game:GetService("Engine")

local ExternalCodeEditor = require("@ed/ExternalCodeEditor")
local Configuration = require("@ed/Configuration")
local Preferences = require("@ed/Preferences")
local Types = require("@ed/Types")

export type CachedProjectInfo = {
    Name: string,
    EngineVersion: string,
    Created: number,
    Saved: number,
}

type StateType = {
    Project: Types.Project,
    CurrentStage: Types.Stage,
    LastAutosaved: number,
    RecentProjects: { string },
    SavedProjectsCache: { CachedProjectInfo }?,
    PlaytestRestoreState: (GameObject & EcDataModel)?,
    ViewportRenderPosition: vector,
    ViewportRenderSize: vector,

    SaveRecents: (StateType) -> (),
    SaveToRecents: (StateType, string) -> (),
    SaveProject: (StateType) -> (),
    OpenProject: (StateType, ManualPath: string?) -> (),
    CreateNewProject: (StateType, Catch: boolean?) -> (),
    ChangeStageTo: (Types.Stage) -> (),
}

local EditorCam = assert(Workspace.SceneCamera)
local WorkspaceLink = GameObject.fromTemplate("TreeLink")
local InterfaceLink = GameObject.fromTemplate("TreeLink")
local ServicesLink = GameObject.fromTemplate("TreeLink")
WorkspaceLink.Name = "WorkspaceLink"
InterfaceLink.Name = "InterfaceLink"
ServicesLink.Name = "ServicesLink"

EditorCam.UseSimpleController = true
WorkspaceLink.Parent = Workspace
InterfaceLink.Parent = Interface
Physics.Simulating = false
Physics:SetSimulationForcePaused(true)
ScriptEngine:SetVMAllowedExecutionTime(_VMNAME, 0)
ScriptEngine:DetachDebuggerFromVM(_VMNAME)

local function saveRecents(State: StateType)
    -- TODO: fix cast
    fs.write(Configuration.RecentProjectsPath, json.encode(State.RecentProjects))
end

local function saveToRecents(State: StateType, Path: string)
    local oldIndex = nil

    for i, v in State.RecentProjects do
        if v == Path then
            oldIndex = i
            break
        end
    end

    if oldIndex then
        table.remove(State.RecentProjects, oldIndex)
    end
    table.insert(State.RecentProjects, Path)
    saveRecents(State)
end

local function stubCoreAssets()
    if not fs.isdirectory("shaders/include") then
        fs.mkdir("shaders/include")
    end

    for _, a in Configuration.CoreAssets do
        if fs.isfile(a) then
            fs.remove(a)
        end

        fs.write(a, "")
    end
end

local function createNewProject_(State: StateType)
    if not fs.isdirectory(Preferences.ProjectsDirectory) then
        fs.mkdir(Preferences.ProjectsDirectory)
    end

    local name = "New Project "
    local nameIndex = 1

    while fs.isdirectory(`{ Preferences.ProjectsDirectory }/{ name .. nameIndex }`) do
        nameIndex += 1

        if nameIndex > 99 then
            error("Tried 99 times to create a project, you need to clean up your Projects folder")
        end
    end

    name = name .. nameIndex
    State.Project.Name = name
    State.Project.IsTempPath = true

    local fullPath = `{ Preferences.ProjectsDirectory }/{ State.Project.Name }`
    fs.mkdir(fullPath)

    fs.definealias("project", fullPath)
    fs.definealias("_projres", "@project/resources")
    fs.setunqualifiedroot("@project/")

    fs.write("@project/phoenix.conf", json.encode({
        GameTitle = State.Project.Name,
        ResourcesDirectory = "resources/",
        RootScene = "scenes/root.world",
        Created = {
            Time = os.time(),
            EngineCommit = Engine.CommitHash,
            EngineVersion = Engine.Version,
        },
        Saved = {
            EditorBuild = Configuration.EditorBuild,
        },
    }))
    fs.write("@project/phoenix.editor", "{}")

    fs.mkdir("@_projres")
    fs.mkdir("@_projres/materials")
    fs.mkdir("@_projres/scenes")
    fs.mkdir("@_projres/sounds")
    fs.mkdir("@_projres/shaders")
    fs.mkdir("@_projres/shaders/include")
    fs.mkdir("@_projres/scripts")
    fs.mkdir("@_projres/textures")
    fs.mkdir("@_projres/textures/materials")

    fs.mkdir("@_projres/scripts/modules")
    fs.write("@_projres/scripts/Main.luau", "print(\"Hello, World!\")\n")

    stubCoreAssets()

    for _, v in { "Smooth", "Checkered", "error" } do
        fs.copy(`@_editres/materials/{v}.mtl`, `@_projres/materials/{v}.mtl`)
    end

    fs.copy("@_editres/textures/materials", "@_projres/textures/materials")
    fs.copy("@_editres/textures/Sky1", "@_projres/textures/Sky1")
    fs.copy("@_editres/textures/image-placeholder.png", "@_projres/textures/image-placeholder.png")
    fs.copy("@_editres/scripts/defaultconfig.luau", "@_projres/scripts/.config.luau")
    fs.copy(Configuration.GetLogo(), "@project/icon.png")

    local dm = GameObject.fromTemplate("DataModel")
    local projWp = GameObject.fromTemplate("Workspace")
    local camera = GameObject.fromTemplate("Camera")
    local dl = GameObject.fromTemplate("DirectionalLight")

    dm.Name = name
    projWp.Name = "Workspace"
    camera.Name = "Camera"
    dl.Name = "DirectionalLight"
    projWp.SceneCamera = camera
    dl.Parent = projWp

    projWp.Parent = dm
    camera.Parent = projWp
    State.Project.DataModel = dm

    State:SaveProject()
    State:OpenProject(State.Project.Name)

    State.SavedProjectsCache = nil
end

local function handleCoreAssetAliasMigration(config)
    local editorconf = json.parse(assert(fs.read("@project/phoenix.conf")))
    assert(typeof(editorconf) == "table")

    local SymlinkMigrationChoices = {
        Migrated = 1,
        RemindLater = 2,
        DoNotMigrate = 3
    }

    local lastSavedData = editorconf.Saved

    if lastSavedData then
        assert(typeof(lastSavedData) == "table")
        lastSavedData = lastSavedData

        if typeof(lastSavedData.EditorBuild) == "number" and lastSavedData.EditorBuild >= 5 then
            return
        end
    end

    if not Engine.IsHeadless then
        imgui.openpopup("Migrate core assets to alias")
        local choiceIndex = 1

        local connection; connection = game.OnFrameBegin:Connect(function()
            if imgui.beginpopupmodal("Migrate core assets to alias") then
                imgui.text("It looks like some core engine assets (UI shaders, `worldUber`, etc) are not currently referred by aliases.")
                imgui.text("This can cause incompatibilities with future versions of the Editor.")
                imgui.text("It is recommended that you migrate these to use the `@base` alias.")

                choiceIndex = imgui.combo("Choice", { "Migrate and alias", "Ignore until next time", "Do not migrate" }, choiceIndex)

                imgui.setnextitemwidth(imgui.contentregionavail().x)
                if imgui.button("Continue") then
                    assert(connection)
                    connection:Disconnect()

                    editorconf = json.parse(assert(fs.read("@project/phoenix.conf")))

                    assert(typeof(editorconf) == "table");
                    editorconf.CoreAssetsSymlinkMigrationChoice = choiceIndex;

                    assert(typeof(editorconf.Saved) == "table")
                    editorconf.Saved.EditorBuild = Configuration.EditorBuild

                    -- TODO luau cast
                    fs.write("@project/phoenix.conf", json.encode(editorconf :: JsonEncodable))

                    if choiceIndex == SymlinkMigrationChoices.Migrated then
                        task.defer(stubCoreAssets)
                    end
                end

                imgui.endpopup()
            else
                assert(connection)
                connection:Disconnect()
            end
        end)
    end
end

local function migrateMaterialsRecursive(start)
    local migrated = 0

    for path, kind in fs.listdir(start) do
        if kind == "f" then
            local contents, error = fs.read(path)

            if not contents then
                warn(`Failed to read { path } for material migration: { error }`)
                continue
            end

            local subs = 0
            local didMigrate = false

            contents, subs = string.gsub(contents, `\"shaders/worldUber\"`, `\"@base/shaders/worldUber.shp\"`)
            didMigrate = didMigrate or subs > 0

            contents, subs = string.gsub(contents, `\"shaders/worldUberTriProjected\"`, `\"@base/shaders/worldUberTriProjected.shp\"`)
            didMigrate = didMigrate or subs > 0

            contents, subs = string.gsub(contents, `\"worldUber\"`, `\"@base/shaders/worldUber.shp\"`)
            didMigrate = didMigrate or subs > 0

            contents, subs = string.gsub(contents, `\"worldUberTriProjected\"`, `\"@base/shaders/worldUberTriProjected.shp\"`)
            didMigrate = didMigrate or subs > 0

            if didMigrate then
                local writeSuccess, writeError = pcall(fs.write, path, contents, false)

                if writeSuccess then
                    migrated += 1
                else
                    warn(`Failed to migrate { path }: { writeError }`)
                end
            end

        elseif kind == "d" then
            migrated += migrateMaterialsRecursive(path)
        end
    end

    return migrated
end

local function handleMaterialMigration(config)
    local editorconf = json.parse(assert(fs.read("@project/phoenix.conf")))
    assert(typeof(editorconf) == "table")

    local lastSavedData = editorconf.Saved

    if lastSavedData then
        assert(typeof(lastSavedData) == "table")
        lastSavedData = lastSavedData

        if typeof(lastSavedData.EditorBuild) == "number" and lastSavedData.EditorBuild >= 6 then
            return
        end
    end

    warn("Migrating materials...")
    local migrated = migrateMaterialsRecursive("materials/")
    print(`Successfully migrated { migrated } materials`)
end

local function openProject_(State: StateType, ManualPath: string?)
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

        for proj, type in fs.listdir(Preferences.ProjectsDirectory) do
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

        game.OnFrameBegin:Wait()
    end

    local projectPath = `{ Preferences.ProjectsDirectory }/{ assert(path) }`

    if fs.isfile(`{ projectPath }/phoenix.lock`) then
        local choice = Engine:ShowMessageBox(
            "Conflict",
            "The project appears to already be open in another instance of the Editor.\n "
                .. "Would you like to open the project anyway?",
            "yesno",
            "warning"
        )

        if choice == 0 then
            return
        end
    end

    fs.write(`{ projectPath }/phoenix.lock`, "This file is just here to mark that the project is open in another instance of the Editor")

    fs.definealias("project", projectPath)
    fs.definealias("_projres", "@project/resources")
    fs.setunqualifiedroot("@project/")
    assert(path)

    if not fs.isdirectory("@project") then
        error(`Directory for project '{ path }' not found: '{ fs.resolvepathabsolute("@project") }'`)
    end

    if not fs.isfile("@project/phoenix.editor") then
        fs.write("@project/phoenix.editor", "{}")
    end

    local config = json.parse(assert(fs.read("@project/phoenix.conf")))
    assert(typeof(config) == "table", "Configuration file is corrupt")

    handleCoreAssetAliasMigration(config)
    handleMaterialMigration(config)

    local objects, error = AssetManager:LoadScene("@_projres/scenes/root.world")

    if not objects then
        error = error or "Unknown error"
        Engine:ShowMessageBox("Project Error", `Couldn't open project '{ path }': { error }`, "ok", "error")

        return
    end

    State.Project.Name = path
    saveToRecents(State, State.Project.Name)

    for _, v in objects do
        if v:HasComponent("DataModel") then
            State.Project.DataModel = v :: any
            break
        end
    end

    if not State.Project.DataModel then
        State.Project.DataModel = GameObject.fromTemplate("DataModel")
        print("Re-created DataModel")
    end

    assert(State.Project.DataModel)
    State.Project.Workspace = State.Project.DataModel:FindChildWithComponent("Workspace")

    if not State.Project.Workspace then
        local newWorkspace = GameObject.fromTemplate("Workspace")

        newWorkspace.SceneCamera = GameObject.fromTemplate("Camera")
        newWorkspace.SceneCamera.Parent = newWorkspace

        newWorkspace.Name = "Workspace"
        newWorkspace.SceneCamera.Name = "Camera"
        newWorkspace.Parent = State.Project.DataModel

        State.Project.Workspace = newWorkspace
        warn("Re-created Workspace")
    end

    assert(State.Project.Workspace)
    if not State.Project.Workspace.SceneCamera then
        local cam = GameObject.fromTemplate("Camera")
        cam.Name = "Camera"
        cam.Parent = State.Project.Workspace
        State.Project.Workspace.SceneCamera = cam

        warn("Re-created camera")
    end

    WorkspaceLink.Target = State.Project.Workspace
    InterfaceLink.Target = State.Project.DataModel:GetService("Interface")
    ServicesLink.Target = State.Project.DataModel
    ServicesLink.Parent = game

    State.Project.DataModel:GetService("Environment")

    History:ClearHistory()
    History.TargetDataModel = State.Project.DataModel

    State.ChangeStageTo("ProjectEditing")

    local module = State.Project.DataModel.LiveScripts
    State.Project.DataModel.LiveScripts = ""

    Engine:BindDataModel(State.Project.DataModel)
    Engine:SetForegroundDataModel(State.Project.DataModel)

    ScriptEngine:CreateVM(Configuration.GameEditingVMName)
    ScriptEngine:DetachDebuggerFromVM(Configuration.GameEditingVMName)

    Engine:SetForegroundDataModel(game)

    ScriptEngine:SetVMAllowedExecutionTime(Configuration.GameEditingVMName, Preferences.ScriptAllowedExecutionTime)

    State.Project.DataModel.LiveScripts = module
    State.Project.HasGitRepository = fs.isdirectory("@project/.git")

    if Preferences.AutoLaunchCodeEditorOnProjectOpen then
        ExternalCodeEditor.Open(State.Project.Name)
    end
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
        local modifiedDate, modifiedDateError = pcall(function()
            local conf = json.parse(assert(fs.read("@project/phoenix.conf")))
            assert(typeof(conf) == "table", `config was a { typeof(conf) } and not a table`);

            conf.Saved = {
                EngineCommit = Engine.CommitHash,
                EngineVersion = Engine.Version,
                EditorBuild = Configuration.EditorBuild,
            }
            fs.write("@project/phoenix.conf", json.encode(conf :: JsonEncodable));

            local edconf = json.parse(assert(fs.read("@project/phoenix.editor")))
            assert(typeof(edconf) == "table", `editor config was a { typeof(conf) } and not a table`);

            edconf.Saved = {
                EngineCommit = Engine.CommitHash,
                EngineVersion = Engine.Version,
                Time = os.time(),
            }
            fs.write("@project/phoenix.editor", json.encode(edconf :: JsonEncodable))
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

local State: StateType = {
    Project = {
        DataModel = nil,
        Workspace = nil,
        Name = "Should not be visible",
        IsTempPath = false,
        HasGitRepository = false,
    },
    RecentProjects = {},
    CurrentStage = "None",
    LastAutosaved = 0,
    ViewportRenderPosition = vector.zero,
    ViewportRenderSize = vector.zero,

    SaveRecents = saveRecents,
    SaveToRecents = saveToRecents,

    SaveProject = function(State: StateType)
        local editingDatamodel = State.Project.DataModel

        if State.CurrentStage == "ProjectPlaying" then
            editingDatamodel = State.PlaytestRestoreState
        end

        tryToSave(editingDatamodel, "@_projres/scenes/root.world")

        if State.CurrentStage == "ProjectPlaying" then
            if Engine:ShowMessageBox("Also save Playtest state?", "Would you also like to save the state of the Playtest world as a separate file?", "yesno", "question", 1) == 1 then
                tryToSave(State.Project.DataModel)
            end
        end
    end,

    OpenProject = function(State: StateType, ManualPath: string?)
        PlayerInput.Cursor = Enum.Cursor.NotAllowed

        xpcall(openProject_, function(err)
            local errorMessage = "Failed to open project: " .. tostring(err)
            warn("Failed to open project: " .. tostring(err))

            Engine:ShowMessageBox("Failed to open project", errorMessage, "ok", "error")
            State.CurrentStage = "SelectProject"
        end, State, ManualPath)

        PlayerInput.Cursor = Enum.Cursor.Arrow
        State.LastAutosaved = game.Time
    end,

    CreateNewProject = function(State: StateType, Catch: boolean?)
        if Catch == nil or Catch == true then
            PlayerInput.Cursor = Enum.Cursor.NotAllowed

            xpcall(createNewProject_, function(err)
                Engine:ShowMessageBox("Failed to create project", "Failed to create new project: " .. tostring(err), "ok", "error")
            end, State)

            PlayerInput.Cursor = Enum.Cursor.Arrow
        else
            createNewProject_(State)
        end
    end,

    ChangeStageTo = function(NewStage: Types.Stage)
        error("Editor state change callback not registered!")
    end,
}

return State
