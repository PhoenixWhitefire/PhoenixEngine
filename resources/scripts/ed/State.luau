-- State.luau, 30/05/2026
local ScriptEngine = game:GetService("ScriptEngine")
local AssetManager = game:GetService("AssetManager")
local PlayerInput = game:GetService("PlayerInput")
local Interface = game:GetService("Interface")
local History = game:GetService("History")
local Physics = game:GetService("Physics")
local Engine = game:GetService("Engine")

local EditorCameraTransform = require("@ed/EditorCameraTransform")
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

local EditorCam = assert(workspace.SceneCamera)
local WorkspaceLink = GameObject.new("TreeLink")
local InterfaceLink = GameObject.new("TreeLink")
local ServicesLink = GameObject.new("TreeLink")
WorkspaceLink.Name = "WorkspaceLink"
InterfaceLink.Name = "InterfaceLink"
ServicesLink.Name = "ServicesLink"

EditorCam.UseSimpleController = true
WorkspaceLink.Parent = workspace
InterfaceLink.Parent = Interface
Physics.Simulating = false
Physics:SetSimulationForcePaused(true)
ScriptEngine:SetVMAllowedExecutionTime(_VMNAME, Configuration.EditorScriptsAllowedExecutionTime)
ScriptEngine:DetachDebuggerFromVM(_VMNAME)

local function saveRecents(State: StateType)
    -- TODO: fix cast
    fs.write(Configuration.RecentProjectsPath, json.encode(State.RecentProjects :: { JsonSerializable }))
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

    State.Project.Name = name .. nameIndex
    State.Project.IsTempPath = true

    local fullPath = `{ Preferences.ProjectsDirectory }/{ State.Project.Name }`
    fs.mkdir(fullPath)

    fs.definealias("project", fullPath)
    fs.definealias("projres", "@project/resources")
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
    fs.copy(Configuration.GetLogo(), "@project/icon.png")

    local dm = GameObject.new("DataModel")
    local projWp = GameObject.new("Workspace")
    local camera = GameObject.new("Camera")
    local dl = GameObject.new("DirectionalLight")

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
	editorconf = editorconf :: { [string]: JsonSerializable }

	local SymlinkMigrationChoices = {
		Migrated = 1,
		RemindLater = 2,
		DoNotMigrate = 3
	}

	local lastSavedData = editorconf.Saved

	if lastSavedData then
		assert(typeof(lastSavedData) == "table")
		lastSavedData = lastSavedData :: { [string]: JsonSerializable }

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
					(editorconf :: any).CoreAssetsSymlinkMigrationChoice = choiceIndex;
					(editorconf :: any).Saved.EditorBuild = Configuration.EditorBuild

					fs.write("@project/phoenix.conf", json.encode(editorconf))

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

    fs.definealias("project", `{ Preferences.ProjectsDirectory }/{ assert(path) }`)
    fs.definealias("projres", "@project/resources")
    fs.setunqualifiedroot("@project/")
    assert(path)

    if not fs.isfile("@project/phoenix.editor") then
        fs.write("@project/phoenix.editor", "{}")
    end

    local config = json.parse(assert(fs.read("@project/phoenix.conf")))
    assert(typeof(config) == "table", "Configuration file is corrupt")

    config = config :: { [string]: JsonSerializable }
    handleCoreAssetAliasMigration(config)

    local objects, error = AssetManager:LoadScene("@projres/scenes/root.world")

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
        State.Project.DataModel = GameObject.new("DataModel")
        print("Re-created DataModel")
    end

    assert(State.Project.DataModel)

    -- TODO luau
    State.Project.Workspace = assert(State.Project.DataModel :: any):FindChildWithComponent("Workspace")
    if not State.Project.Workspace then
        local newWorkspace = GameObject.new("Workspace")

        newWorkspace.SceneCamera = GameObject.new("Camera")
        newWorkspace.SceneCamera.Parent = State.Project.Workspace

        newWorkspace.Name = "Workspace"
        newWorkspace.SceneCamera.Name = "Camera"

        State.Project.Workspace = newWorkspace;
        warn("Re-created Workspace")
    end

    assert(State.Project.Workspace)
    if not State.Project.Workspace.SceneCamera then
        local cam = GameObject.new("Camera")
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
    ScriptEngine:CreateVM(Configuration.GameEditingVMName)
    Engine:BindDataModel(game)

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

            (conf :: { [string]: JsonSerializable }).Saved = {
                EngineCommit = Engine.CommitHash,
                EngineVersion = Engine.Version,
                EditorBuild = Configuration.EditorBuild,
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

            EditorCameraTransform.Save(workspace.SceneCamera)
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

        tryToSave(editingDatamodel, "@projres/scenes/root.world")

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
