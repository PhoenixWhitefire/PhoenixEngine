-- ed.luau, 14/09/2025
local DeveloperTools = game:GetService("DeveloperTools")
local AssetManager = game:GetService("AssetManager")
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
local REIMPORTABLE_MODEL = "Ed_ReimportableModel"

local PluginManager = require("./PluginManager.luau")
local HistoryView = require("./HistoryView.luau")
local TagViewer = require("./TagEditor.luau")
local CoreTools = require("./CoreTools.luau")
local Types = require("./Types.luau")
type Stage = Types.Stage

local CurrentStage: Stage = "None"
local Stages: { [string]: ((number) -> ()) & ((string) -> (any)) } = {}
local StageUpdateSignal: EventConnection?

local EditorCam = assert(workspace.SceneCamera)
local EditorTl = GameObject.new("TreeLink")
local InterfaceLink = GameObject.new("TreeLink")

local Project: Types.Project = {
	DataModel = nil,
	Workspace = nil,
	Name = "Should not be visible",
	IsTempPath = false,
	HasGitRepository = false,
}
local restoreState = nil
local SyncCoreAssetsOptions = { "AlwaysAsk" :: "AlwaysAsk", "AutomaticallyUpdate" :: "AutomaticallyUpdate", "DoNotUpdate" :: "DoNotUpdate" }

local RecentProjects: { string } = {}
local EditorPreferences = {
	Version = 1,
	AutoLaunchCodeEditorOnProjectOpen = false,
	ClearOutputOnProjectOpen = true,
	ClearOutputOnPlaytest = true,
	CodeEditorPath = nil :: string?,
	UseLightTheme = false,
	ClosingProjectSaveRequiresConfirmation = false,
	AutosaveInterval = 5,
	KeepGizmoInView = false,
	InvertTabButtonLitColors = false,
	SyncCoreAssets = "AlwaysAsk" :: index<typeof(SyncCoreAssetsOptions), number>,
	DevToolsOpen = { "Explorer", "Properties", "Scripts" },
	ToolsOpen = { BottomBar = true, Output = true },
	AutoReimportModels = true,
	GitIntegration = false,
}

EditorPreferences = EditorPreferences :: { [keyof<typeof(EditorPreferences)>]: index<typeof(EditorPreferences), keyof<typeof(EditorPreferences)>> } & typeof(EditorPreferences)

local EDITOR_RECENT_PROJECTS_PATH = "./editor/recent-projects.json"

local GameEditingVM = "GameEditingVM"
local GameVM = "GameVM"

EditorCam.UseSimpleController = true
EditorTl.Parent = workspace
InterfaceLink.Parent = Interface
Physics.Simulating = false

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
local WasHoldingF5 = false
local WasHoldingCtrlKeyDown = false
local WasHoldingZKeyDown = false
local WasHoldingYKeyDown = false
local ErrorMessage = ""
local ErrorMessageTimeRemaining = 0
local LastAutosaved = 0
local ViewportRenderPosition = vector.zero
local ViewportRenderSize = vector.zero

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

for _, devTool in EditorPreferences.DevToolsOpen do
	DeveloperTools:SetToolEnabled(devTool, true)
end

if not Engine.IsHeadless then
	imgui.stylecolors(if EditorPreferences.UseLightTheme then 'l' else 'd')
end

fs.definealias("editres", "@cwd/resources")
fs.definealias("etextures", "@editres/textures")

if not fs.isdirectory("@cwd/editor") then
	fs.mkdir("@cwd/editor")
end

if not fs.isdirectory("@cwd/projects") then
	fs.mkdir("@cwd/projects")
end

local ReimportWatchers: { [string]: { Disconnect: () -> (), Models: { GameObject & Model } }? } = {}
local ModelModifiedTimes: { [string]: number } = {}

if fs.isfile("./editor/models.json") then
	ModelModifiedTimes = json.parse(assert(fs.read("./editor/models.json"))) :: { [string]: number }
end

local function setErrorNotification(Error: string)
	ErrorMessage = Error
	ErrorMessageTimeRemaining = 2
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
			local success: boolean, error: string? = (pcall::any)(function()
				History:Undo()
			end)

			if not success then
				assert(error)

				local realError = error:sub(error:find(":") or 1, #error)
				realError = realError:sub((realError:find(" ") or 0) + 1, #realError)

				setErrorNotification("Undo failed: " .. realError)
			end
		end
	end

	if ctrlKeyDown and yKeyDown then
		if not WasHoldingCtrlKeyDown or not WasHoldingYKeyDown then
			local success: boolean, error: string? = (pcall::any)(function()
				History:Redo()
			end)

			if not success then
				assert(error)

				local realError = error:sub(error:find(":") or 1, #error)
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
	MouseClick = (not WasHoldingDownM1) and PlayerInput:IsMouseButtonPressed(Enum.MouseButton.Left)
	local f5 = PlayerInput:IsKeyPressed(Enum.Key.F5)

	if not History.IsRecordingAction then
		if CurrentStage == "ProjectEditing" then
			if f5 and not WasHoldingF5 then
				changeStageTo("ProjectPlaying")
			end

			handleUndoRedoKeybinds()

		elseif CurrentStage == "ProjectPlaying" then
			if f5 and not WasHoldingF5 then
				changeStageTo("ProjectEditing")
			end
		end
	end

	WasHoldingF5 = f5

	CoreTools.Update(Project, EditorPreferences, ViewportRenderPosition, ViewportRenderSize, DeltaTime)
	Stages[CurrentStage](DeltaTime)

	WasHoldingDownM1 = PlayerInput:IsMouseButtonPressed(Enum.MouseButton.Left)

	if ErrorMessageTimeRemaining > 0 then
		imgui.settooltip(ErrorMessage)
		ErrorMessageTimeRemaining -= DeltaTime
	end
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
	fs.write(EDITOR_RECENT_PROJECTS_PATH, json.encode(RecentProjects :: { JsonSerializable }))
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
				Time = os.time(),
				EngineCommit = Engine.CommitHash,
				EngineVersion = Engine.Version
			}
			fs.write("@project/phoenix.conf", json.encode(conf));
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

local function saveProject()
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
		local result = fs.execute(`{ EditorPreferences.CodeEditorPath } "{ fs.cwd() }/projects/{ Project.Name }"`)

		if #result > 1 then
			warn("Open code editor result: ", result)
		end
	end
end

local function syncCoreAssets()

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

		for proj, type in fs.listdir("@cwd/projects") do
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

	fs.definealias("project", "@cwd/projects/" .. assert(path))
	fs.definealias("projres", "@project/resources")
	fs.setunqualifiedroot("@project/")
	assert(path)

	local config = json.parse(assert(fs.read("@project/phoenix.conf")))
	assert(typeof(config) == "table", "Configuration file is corrupt")

	config = config :: { [string]: JsonSerializable }
	local coreAssetsOutOfDate = false;

	--if EditorPreferences.SyncCoreAssets == "AlwaysAsk" or EditorPreferences.SyncCoreAssets == "AutomaticallyUpdate" then
	if false then
		if typeof(config.Saved) ~= "table" then
			coreAssetsOutOfDate = true
		else
			local lastSavedWithCommit = (config.Saved :: { [string]: JsonSerializable }).EngineCommit

			if typeof(lastSavedWithCommit) ~= "string" or lastSavedWithCommit ~= Engine.CommitHash then
				coreAssetsOutOfDate = true
			end
		end
	end

	if coreAssetsOutOfDate then
		local shouldSyncCoreAssets = false

		if EditorPreferences.SyncCoreAssets == "AlwaysAsk" then
			imgui.openpopup("Core assets out-of-date")
			local co = coroutine.running()

			local connection
			connection = game.OnFrameBegin:Connect(function()
				if imgui.beginpopupmodal("Core assets out-of-date", true) then
					imgui.text("Built-in shaders for this project, such as `worldUber` and `postprocessing` may be out-of-date to the Editor.")

					imgui.endpopup()
				else
					assert(connection)
					connection:Disconnect()
					coroutine.resume(co)
				end
			end)

			coroutine.yield()

		elseif EditorPreferences.SyncCoreAssets == "AutomaticallyUpdate" then
			shouldSyncCoreAssets = true
		end

		if shouldSyncCoreAssets then
			syncCoreAssets()
		end
	end

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

	Project.Workspace = Project.DataModel:FindChildWithComponent("Workspace")
	Project.Workspace.Name = "Workspace"

	if not Project.Workspace then
		-- TODO luau: Project.Workspace is refining to `nil`
		Project.Workspace = GameObject.new("Workspace")
		Project.Workspace.SceneCamera = GameObject.new("Camera")
		Project.Workspace.SceneCamera.Parent = Project.Workspace

		Project.Workspace.Name = "Workspace"
		Project.Workspace.SceneCamera.Name = "Camera"

		print("Re-created Workspace")
	end

	assert(Project.Workspace)
	if not Project.Workspace.SceneCamera then
		local cam = GameObject.new("Camera")
		cam.Name = "Camera"
		cam.Parent = Project.Workspace
		Project.Workspace.SceneCamera = cam
	end

	EditorCam.Transform = Project.Workspace.SceneCamera.Transform
	EditorTl.Target = Project.Workspace
	InterfaceLink.Target = Project.DataModel:GetService("Interface")

	Project.DataModel:GetService("Environment")

	History:ClearHistory()
	History.TargetDataModel = Project.DataModel

	changeStageTo("ProjectEditing")

	local module = Project.DataModel.LiveScripts
	Project.DataModel.LiveScripts = ""

	Engine:BindDataModel(Project.DataModel)
	Engine:CreateVM(GameEditingVM)
	Engine:BindDataModel(game)

	Project.DataModel.LiveScripts = module
	Project.HasGitRepository = fs.isdirectory("@project/.git")

	if EditorPreferences.AutoLaunchCodeEditorOnProjectOpen then
		openCodeEditor()
	end
end

local function openProject(ManualPath: string?)
	PlayerInput.Cursor = Enum.Cursor.NotAllowed

	xpcall(openProject_, function(err)
		Engine:ShowMessageBox("Failed to open project", "Failed to open project: " .. tostring(err), "ok", "error")
	end, ManualPath)

	PlayerInput.Cursor = Enum.Cursor.Arrow
	LastAutosaved = game.Time
end

local SavedProjectsCache: { string }? = nil

local function createNewProject_()
	if not fs.isdirectory("@cwd/projects") then
		fs.mkdir("@cwd/projects")
	end

	local name = "New Project "
	local nameIndex = 1

	while fs.isdirectory("@cwd/projects/" .. name .. nameIndex) do
		nameIndex += 1

		if nameIndex > 99 then
			error("Tried 99 times to create a project, you need to clean up your Projects folder")
		end
	end

	Project.Name = name .. nameIndex
	Project.IsTempPath = true

	local fullPath = "@cwd/projects/" .. assert(Project.Name)
	fs.mkdir(fullPath)

	fs.definealias("project", fullPath)
	fs.definealias("projres", "@project/resources")

	fs.write(fullPath .. "/phoenix.conf", json.encode({
		GameTitle = Project.Name,
		ResourcesDirectory = "resources/",
		RootScene = "scenes/root.world",
		Engine = {
			Version = Engine.Version,
			Commit = Engine.CommitHash
		},
		Created = {
			Time = os.time(),
			EngineCommit = Engine.CommitHash,
			EngineVersion = Engine.Version
		}
	}))

	fs.mkdir("@projres")
	fs.mkdir("@projres/materials")
	fs.mkdir("@projres/scenes")
	fs.mkdir("@projres/sounds")
	fs.mkdir("@projres/shaders")
	fs.mkdir("@projres/scripts")
	fs.mkdir("@projres/textures")
	fs.mkdir("@projres/textures/materials")

	fs.mkdir("@projres/scripts/modules")
	fs.write("@projres/scripts/Main.luau", "print(\"Hello, World!\")\n")

	for _, v in { "error", "worldUber", "worldUberTriProjected", "worldUberSkinned", "skybox", "boxframe", "particle", "postprocessing", "ui" } do
		if fs.isfile(`@editres/shaders/{v}.shp`) then
			fs.copy(`@editres/shaders/{v}.shp`, `@projres/shaders/{v}.shp`)
		end
		if fs.isfile(`@editres/shaders/{v}.vert`) then
			fs.copy(`@editres/shaders/{v}.vert`, `@projres/shaders/{v}.vert`)
		end
		if fs.isfile(`@editres/shaders/{v}.frag`) then
			fs.copy(`@editres/shaders/{v}.frag`, `@projres/shaders/{v}.frag`)
		end
		if fs.isfile(`@editres/shaders/{v}.geom`) then
			fs.copy(`@editres/shaders/{v}.geom`, `@projres/shaders/{v}.geom`)
		end
	end

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

	saveProject()
	openProject(Project.Name)

	SavedProjectsCache = nil
end

local function createNewProject()
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

	imgui.pushstylecolor(3, backgroundColor)

	imgui.beginchild(Text .. "_Selection", 120, 150, "b")

	local clicked, hovered = false, false
	local cpos = imgui.cursorposition()
	imgui.setcursorposition(cpos.x + 2, cpos.y)

	imgui.image(Icon, vector.create(100, 100), false, tint, false)
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

local function watchModelForReimporting(reimportable: GameObject & Model)
	local watcher = ReimportWatchers[reimportable.ImportPath]

	if watcher then
		table.insert(watcher.Models, reimportable)
		return
	end

	watcher = {
		Models = { reimportable },
		Disconnect = fs.watch(reimportable.ImportPath, function(_, EventType)
			if EventType ~= Enum.FileSystemEvent.Modified then
				return
			end

			if not EditorPreferences.AutoReimportModels then
				return
			end

			assert(watcher)
			print(`Re-importing { reimportable.ImportPath }`)

			local imported = AssetManager:ImportModel(reimportable.ImportPath)
			assert(imported, "Re-import failed")

			local action = History:TryBeginAction("Auto re-import during session")

			for _, model in watcher.Models :: { GameObject & Model } do
				-- preserve tags, name, parent, object ID, treelinks

				for _, child in model:GetChildren() do
					child.Parent = nil -- Allow undo
				end

				for _, newChild in imported:GetChildren() do
					newChild:Duplicate().Parent = model
				end
			end

			if action then
				History:FinishAction(action)
			end

			ModelModifiedTimes[reimportable.ImportPath] = fs.lastwritten(reimportable.ImportPath)
		end)
	}
	ReimportWatchers[reimportable.ImportPath] = watcher
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
				local imported = AssetManager:ImportModel(paths[1])

				-- TODO type of `imported` becomes `nil` after this clause for some reason
				if not imported then
					setErrorNotification("Failed to import")
					if importAction then
						History:DiscardAction(importAction)
					end

					return
				end

				assert(Project.Workspace)
				imported:AddTag(REIMPORTABLE_MODEL)
				imported.Parent = Project.Workspace
				DeveloperTools:SetExplorerSelections({ imported })

				if importAction then
					History:FinishAction(importAction)
				end

				ModelModifiedTimes[paths[1]] = fs.lastwritten(paths[1])
				watchModelForReimporting(imported)
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

local function saveSettings()
	local success, error = pcall(function()
		EditorPreferences.DevToolsOpen = {}

		for _, name in DeveloperTools:GetToolNames() do
			if DeveloperTools:IsToolEnabled(name) then
				table.insert(EditorPreferences.DevToolsOpen, name)
			end
		end

		fs.write("./editor/settings.json", json.encode(EditorPreferences))

		if openSettingsMenuToConfigureCodeEditor and EditorPreferences.CodeEditorPath and #EditorPreferences.CodeEditorPath > 0 then
			task.defer(openCodeEditor)
		end

		openSettingsMenuToConfigureCodeEditor = false
		didFocusCodeEditorSetting = false

		fs.write("./editor/models.json", json.encode(ModelModifiedTimes :: { [string]: JsonSerializable }))
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

local SPYWARE_SETTINGS_URL = "https://github.com/PhoenixWhitefire/PhoenixEngine/issues/30"
local GITHUB_URL = "https://github.com/PhoenixWhitefire/PhoenixEngine"

local HoveringOverMenuBar = false
local HoveringOverRibbonTab = false
local RibbonPopupDistance = 18

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

local function exportProject()
	saveProject()

	local enginePath = findEnginePath()

	if not enginePath then
		print("Export cancelled")
		return
	end

	imgui.openpopup("Export Project")
	local exportName = Project.Name
	local exportDir = fs.promptopenfolder("Choose export directory")
	local exportTo = `{exportDir}/{exportName}`

	if not exportDir then
		print("No export directory selected, export cancelled")
		return
	end
	
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

		exportTo = `{exportDir}/{exportName}`

		imgui.separator()

		imgui.text("Your project will export to:")
		imgui.pushstylecolor(0, { 1, 1, 1, 1 })
		imgui.text(exportTo)
		imgui.popstylecolor()

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

	fs.copy(enginePath, `{exportTo}/{Project.Name}{if enginePath:find(".exe") then ".exe" else ""}`)

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
		printTable(data :: { [string]: index<typeof(data), keyof<typeof(data)>> }, "", nil, warn)
		fetchCommitHashData = data
	end
end

local function textCentered(text, cursor, cxavail)
	imgui.setcursorposition(getCenteredX(cursor.x, cxavail, imgui.textsize(text).x), cursor.y)
	imgui.text(text)
end

local wasSettingsOpen = false
local OpenGitCommitMenu = false
local GitStatusResult = ""
local GitCommitMessage = ""

local function getGitStatus()
	GitStatusResult = fs.execute(`git -C "{ fs.resolvepath("@project") }" status`)
end

local function renderMainMenuBar(dt: number)
	local pushedCol = CurrentStage == "ProjectPlaying"

	if pushedCol then
		imgui.pushstylecolor(13, {0, 56/270, 15/270, 1})
	end

	local render = imgui.beginmainmenubar()

	if pushedCol then
		imgui.popstylecolor()
	end

	if not render then
		return
	end

	local mainMenuCursorY = PlayerInput:GetCursorPosition().y
	HoveringOverMenuBar = mainMenuCursorY < RibbonPopupDistance

	local openAboutWindow = false
	local openSettingsWindow = false
	
	if imgui.beginmenu("File") then
		if CurrentStage == "ProjectEditing" then
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
				task.defer(openCodeEditor)
			end
		end

		if imgui.menuitem("Save", CurrentStage == "ProjectEditing") then
			saveProject()
		end

		if CurrentStage == "ProjectEditing" then
			if imgui.menuitem("Project Settings") then
				task.defer(function()
					imgui.openpopup("Project Settings")

					local newProjectName = Project.Name

					while true do
						if not imgui.beginpopupmodal("Project Settings", true) then
							break
						end

						newProjectName = imgui.inputstring("Project Name", newProjectName)

						if newProjectName:match("[^%w%s]") then
							imgui.text("Only a-z, A-Z, 0-9 (alphanumeric) is allowed")
						end

						if newProjectName ~= Project.Name and imgui.button("Save new name") then
							local buttonSelection = Engine:ShowMessageBox("Rename project?", "Are you sure you want to rename the project?", "yesno")

							if buttonSelection == 1 then
								local prevName = Project.Name

								fs.rename("@cwd/projects/" .. Project.Name, newProjectName)
								fs.definealias("project", "@cwd/projects/" .. newProjectName)

								Project.Name = newProjectName

								local conf = json.parse(assert(fs.read("@project/phoenix.conf")))
								assert(typeof(conf) == "table")
								-- TODO: fix cast
								conf["GameTitle" :: string] = newProjectName

								fs.write(
									"@project/phoenix.conf",
									json.encode(conf)
								)

								table.remove(RecentProjects, table.find(RecentProjects, prevName))
								saveToRecents(newProjectName)
							else
								newProjectName = Project.Name
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
									local path = fs.promptopen("@etextures/", { "*.png" , "*.jpg", "*.jpeg" }, "Images")[1]

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

						imgui.endpopup()

						game.OnFrameBegin:WaitUntil()
					end
				end)
			end

			if EditorPreferences.GitIntegration and imgui.beginmenu("Git") then
				if not Project.HasGitRepository then
					if imgui.menuitem("Initialize repository") then
						task.defer(function()
							print(fs.execute(`git -C "{ fs.resolvepath("@project") }" init`))
							Project.HasGitRepository = fs.isdirectory("@project/.git")
						end)
					end
				else
					if imgui.menuitem("Commit") then
						task.defer(function()
							getGitStatus()
							OpenGitCommitMenu = true
						end)
					end
				end

				imgui.endmenu()
			end

			if imgui.menuitem("Export") then
				task.defer(exportProject)
			end

			if imgui.menuitem("Close") then
				-- TODO luau cast
				task.defer(changeStageTo, "SelectProject" :: "SelectProject")
			end
		end

		if imgui.menuitem("Open", CurrentStage == "SelectProject") then
			SavedProjectsCache = nil
			task.defer(openProject, nil) -- TODO type error when `nil` is not supplied
		end

		if imgui.menuitem("New", CurrentStage == "SelectProject") then
			task.defer(createNewProject)
		end

		if imgui.menuitem("About") then
			openAboutWindow = true
		end

		if imgui.menuitem("Editor Settings") then
			openSettingsWindow = true
		end

		imgui.endmenu()
	end

	if OpenGitCommitMenu then
		imgui.openpopup("Commit")
		OpenGitCommitMenu = false
	end

	if imgui.beginpopupmodal("Commit", true) then
		imgui.text(GitStatusResult)
		imgui.separator()

		if GitStatusResult:find("nothing to commit") then
			imgui.text("No changes to commit.")
		else
			local tsy = imgui.textsize("").y

			local cavail = imgui.contentregionavail()
			local cxavail = cavail.x
			local cursor = imgui.cursorposition()

			imgui.setcursorposition(vector.create(cursor.x, cavail.y + tsy * 5))
			imgui.dummy()

			imgui.text("All files will be committed automatically.\nCommit message:")
			GitCommitMessage = imgui.inputstring("##CommitMessage", GitCommitMessage)

			if #GitCommitMessage == 0 then
				imgui.text("Please enter a commit message.")
			end

			cursor = imgui.cursorposition()
			local buttonText = "Commit"
			imgui.setcursorposition(getCenteredX(cursor.x, cxavail, imgui.textsize(buttonText).x), imgui.windowsize().y - tsy * 2 - 4)

			if imgui.button(buttonText) then
				task.defer(function()
					GitCommitMessage = string.gsub(GitCommitMessage, '"', '\\"')
					print(fs.execute(`git -C { fs.resolvepath("@project") } commit -m "{ GitCommitMessage }" -a`))
					getGitStatus()
				end)
			end
		end

		imgui.endpopup()
	end

	if CurrentStage ~= "SelectProject" then
		imgui.separator()

		WipTabButtonHovering = nil
		WindowHovering = false

		for i, tab in RibbonTabs do
			if imgui.menuitem(if CurrentRibbonTab == i then `[{tab.Name}]` else tab.Name) then
				CurrentRibbonTab = i
			end
		end

		if (HoveringOverMenuBar or HoveringOverRibbonTab or TabButtonHovering ~= nil or WindowHovering) and not PlayerInput:IsMouseButtonPressed(Enum.MouseButton.Right) then
			local windowWidth = Renderer.Resolution.x

			local TAB_HEIGHT = 175

			imgui.setnextwindowposition(0, 16)
			imgui.setnextwindowsize(windowWidth, TAB_HEIGHT)

			imgui.begin("RibbonTabContents", "nt | nr | nm | nc | ns | h")
			HoveringOverRibbonTab = imgui.windowhovered() or imgui.anyitemactive()
			RibbonTabs[CurrentRibbonTab].Renderer(dt)
			imgui.endw()

			RibbonPopupDistance = TAB_HEIGHT -- fixes ribbon closing when enabling tools
		else
			RibbonPopupDistance = 18
		end

		TabButtonHovering = WipTabButtonHovering
	end

	imgui.endmainmenubar()

	if openAboutWindow then
		imgui.openpopup("About")

		local ws = Renderer.Resolution

		local ABOUT_SIZE_X, ABOUT_SIZE_Y = 375, 340

		imgui.setnextwindowposition((ws.x - ABOUT_SIZE_X) / 2, (ws.y - ABOUT_SIZE_Y) / 2);
		imgui.setnextwindowsize(ABOUT_SIZE_X, ABOUT_SIZE_Y);
	end

	if imgui.beginpopupmodal("About", true) then
		local cxavail = imgui.contentregionavail().x
		local cursor = imgui.cursorposition()

		local LOGO_SIZE = 150

		imgui.setcursorposition(getCenteredX(cursor.x, cxavail, LOGO_SIZE), cursor.y)
		imgui.image(LOGO_IMAGE, vector.create(LOGO_SIZE, LOGO_SIZE))

		cursor += vector.create(0, 160)

		local tts = imgui.textsize("")

		textCentered("Phoenix Engine Editor", cursor, cxavail)
		cursor += vector.create(0, tts.y + 2)

		textCentered(Engine.Version, cursor, cxavail)
		cursor += vector.create(0, tts.y + 15)

		imgui.setcursorposition(getCenteredX(cursor.x, cxavail, imgui.textsize(GITHUB_URL).x), cursor.y)
		imgui.urllink(GITHUB_URL)
		cursor += vector.create(0, tts.y + 2)

		local commitText = Engine.CommitHash:sub(1, 8)

		imgui.setcursorposition(getCenteredX(cursor.x, cxavail, imgui.textsize(commitText).x), cursor.y)
		imgui.urllink(commitText, `{ GITHUB_URL }/commit/{ Engine.CommitHash }`)
		cursor += vector.create(0, tts.y + 16)

		if not isFetchingLatestCommitData and not latestCommit then
			isFetchingLatestCommitData = true
			task.defer(getLatestGitHubCommit)
		end

		if latestCommit then
			if latestCommit ~= Engine.CommitHash then
				textCentered("There is a newer version available.", cursor, cxavail)
				cursor += vector.create(0, tts.y + 2)

				textCentered(`Latest: { latestCommit:sub(1, 8) }`, cursor, cxavail)
				cursor += vector.create(0, tts.y + 4)

				local buttonText = "Download"
				imgui.setcursorposition(getCenteredX(cursor.x, cxavail, imgui.textsize(buttonText).x), cursor.y)

				if canUpdate and imgui.button(buttonText) then
					if CurrentStage ~= "SelectProject" then
						Engine:ShowMessageBox("Update", "You must close the current project to update")
					elseif fs.isfile("./update.zip") then
						Engine:ShowMessageBox("Update", "Delete or rename the 'update.zip' file in the Editor's installation directory to proceed", "ok", "error")
					else
						imgui.closecurrentpopup()
						changeStageTo("Update")
					end
				end
			else
				local urutdText = "You are on the latest available version."
				local urutdtsx = imgui.textsize(urutdText).x

				imgui.setcursorposition(getCenteredX(cursor.x, cxavail, urutdtsx), cursor.y + 32)
				imgui.text(urutdText)
			end
		elseif fetchCommitHashData then
			textCentered("Failed to check for updates.", cursor, cxavail)
			cursor += vector.create(0, tts.y + 2)
			textCentered(`Status: { fetchCommitHashData.Status }`, cursor, cxavail)
			cursor += vector.create(0, tts.y + 2)
			textCentered(`Error: { fetchCommitHashData.Body }`, cursor, cxavail)
			imgui.setitemtooltip(fetchCommitHashData.Body)
		end

		imgui.endpopup()
	end

	if openSettingsWindow or openSettingsMenuToConfigureCodeEditor then
		imgui.openpopup("Editor Settings")

		local SETTINGS_SIZE_X, SETTINGS_SIZE_Y = 420, 140

		local ws = Renderer.Resolution

		imgui.setnextwindowposition((ws.x - SETTINGS_SIZE_X) / 2, (ws.y - SETTINGS_SIZE_Y) / 2)
		imgui.setnextwindowsize(SETTINGS_SIZE_X, SETTINGS_SIZE_Y)
	end

	if imgui.beginpopupmodal("Editor Settings", true) then
		local lightMode = imgui.checkbox("Light", EditorPreferences.UseLightTheme)
		if lightMode ~= EditorPreferences.UseLightTheme then
			imgui.stylecolors(if lightMode then "l" else "d")
		end
		EditorPreferences.UseLightTheme = lightMode

		if openSettingsMenuToConfigureCodeEditor and not didFocusCodeEditorSetting then
			imgui.focuskeyboardhere()
			didFocusCodeEditorSetting = true
		end

		EditorPreferences.CodeEditorPath = imgui.inputstring("Code Editor path", EditorPreferences.CodeEditorPath or "")

		EditorPreferences.AutoLaunchCodeEditorOnProjectOpen = imgui.checkbox("Launch code editor on project open", EditorPreferences.AutoLaunchCodeEditorOnProjectOpen)

		EditorPreferences.ClearOutputOnProjectOpen = imgui.checkbox("Clear Output on Project open", EditorPreferences.ClearOutputOnProjectOpen)
		EditorPreferences.ClearOutputOnPlaytest = imgui.checkbox("Clear Output on Playtest", EditorPreferences.ClearOutputOnPlaytest)

		EditorPreferences.ClosingProjectSaveRequiresConfirmation = imgui.checkbox(
			"Require confirmation for save when closing Project through `File` > `Close`",
			EditorPreferences.ClosingProjectSaveRequiresConfirmation
		)

		EditorPreferences.AutosaveInterval = imgui.inputnumber("Autosave interval (in minutes)", EditorPreferences.AutosaveInterval)
		EditorPreferences.KeepGizmoInView = imgui.checkbox("Turn the camera to keep the gizmo handles in view while moving an object", EditorPreferences.KeepGizmoInView)
		EditorPreferences.InvertTabButtonLitColors = imgui.checkbox("Invert tab button active colors", EditorPreferences.InvertTabButtonLitColors)
		--EditorPreferences.SyncCoreAssets = SyncCoreAssetsOptions[imgui.combo("Sync core assets", SyncCoreAssetsOptions :: { string }, assert(table.find(SyncCoreAssetsOptions, EditorPreferences.SyncCoreAssets)))]
		EditorPreferences.GitIntegration = imgui.checkbox("Git integration", EditorPreferences.GitIntegration)

		imgui.urllink("https://youtu.be/YCjNT9qGjh4?t=7193")

		if imgui.treenode("##") then
			imgui.urllink("Configure spyware settings", SPYWARE_SETTINGS_URL)
			imgui.treepop()
		end

		imgui.endpopup()
		wasSettingsOpen = true

	elseif wasSettingsOpen then
		wasSettingsOpen = false
		saveSettings()
	end
end

local PrevSelections: { GameObject & Transform } = {}

local SelectionPart: GameObject & Mesh & RigidBody & Transform = GameObject.new("Mesh")
SelectionPart.Material = "boxframe"
SelectionPart.Tint = Color.new(166/255, 200/255, 1)
SelectionPart.PhysicsCollisions = false
SelectionPart.FaceCulling = Enum.FaceCulling.None

local CommandBarCommand = ""
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

Logging.OnMessaged:Connect(function(Type, Message, ExtraTags, Value)
	if type(Value) == "table" then
		table.freeze(Value :: any)
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
		Time = os.date("%H:%M:%S"),
		Source = source,
		Line = line,
		OpenTables = {},
		TablesWithAnyElements = {},
		TablePrettyOpen = {},
		TablePrettyChangeOpen = {}
	}

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

PlayerInput.KeyEvent:Connect(function(kie: KeyInputEvent)
	if not CommandBarActive or kie.Action ~= Enum.InputAction.Pressed or kie.Key ~= Enum.Key.Enter then
		return
	end

	local commandBarAction = History:TryBeginAction("CommandBarCode")

	if #CommandBarCommand > 1 and CommandBarCommand:sub(1, 1) == "=" then
		CommandBarCommand = `print({ CommandBarCommand:sub(2, #CommandBarCommand) })`
	end

	local success = Engine:RunInVM(if CurrentStage == "ProjectEditing" then GameEditingVM else GameVM, CommandBarCommand, "@@projres/scripts/!CommandBar")

	if not success then
		local cmd = CommandBarCommand
		task.wait() -- wait for command bar to be un-focused, then show the command that ran
		CommandBarCommand = cmd
	end

	if commandBarAction then
		History:FinishAction(commandBarAction)
	end
end)

local function renderOutput()
	local renderOutput = false
	if EditorPreferences.ToolsOpen.Output then
		renderOutput, EditorPreferences.ToolsOpen.Output = imgui.begin("Output", "h", true)
	end

	if renderOutput then
		local logMessageHoveringWip: number = -1
		local smid = 0
		local rightClickedLogMessage = false

		for mi, message in Log do
			imgui.pushstylecolor(0, { 0.35, 0.35, 0.35, 1 })

			imgui.text(message.Time)

			imgui.popstylecolor()

			local LogColors = {
				[Enum.LogType.None]    = { 1, 1, 1, 1 },
				[Enum.LogType.Info]    = { 1, 1, 1, 1 },
				[Enum.LogType.Warning] = { 255 / 255, 171 / 255, 61 / 255, 1 }, -- warning
				[Enum.LogType.Error]   = { 255 / 255, 61 / 255, 61 / 255, 1 }   -- error
			}

			local logColor = LogColors[message.Type]
			imgui.pushstylecolor(0, logColor)
			imgui.pushstylecolor(52, logColor)

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
									renderTable(v, depth + 1, tpos)
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
											DeveloperTools:SetExplorerSelections({ v })
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
					renderTable(subMessage.DirectValue, 0)
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

						imgui.pushstylecolor(0, { 0.35, 0.35, 0.35, 1 })

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
				Log = {}
			end

			imgui.endpopup()
		end

		LogMessageHovering = logMessageHoveringWip
		imgui.endw()
	end
end

local function renderViewport(DeltaTime: number)
	if CurrentStage == "ProjectEditing" and Project.Workspace then
		local mouseVec = workspace:ScreenPointToVector(PlayerInput:GetCursorPosition(), 1000)
		local rayresult = if not PlayerInput.IsMouseSunk
							then Project.Workspace:Raycast(workspace.SceneCamera.Transform.Position, mouseVec, { SelectionPart })
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

		local sel = (sel_ :: any) :: GameObject & Transform
		selmap[sel.ObjectId] = sel

		if not PrevSelections[sel.ObjectId] then
			local new = SelectionPart:Duplicate()
			-- TODO luau report bug
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

	PluginManager.OnUpdate(DeltaTime, CurrentStage)
	CoreTools.Update(Project, EditorPreferences, ViewportRenderPosition, ViewportRenderSize, DeltaTime)
	HistoryView.Render()
	TagViewer.Render()

	if EditorPreferences.ToolsOpen.BottomBar then
		local w = Renderer.Resolution

		imgui.setnextwindowposition(0, w.y - 35)
		imgui.setnextwindowsize(w.x, 35)

		imgui.begin("Bottom Bar", "nt | nd | nr | ns")

		imgui.setcursorposition(imgui.cursorposition() + vector.create(0, 2))
		imgui.text("> ")
		imgui.sameline()
		imgui.setcursorposition(imgui.cursorposition() - vector.create(0, 2))

		CommandBarCommand = imgui.inputstring("##Command", CommandBarCommand)
		CommandBarActive = imgui.itemactive()

		imgui.sameline()

		local noteStr = `{ Engine.Framerate } FPS ({ CurrentStage }) | { Project.Name }`
		imgui.text(noteStr)
		imgui.setitemtooltip(noteStr)

		imgui.endw()
	end

	renderOutput()

	local canInteract = IsHoveringViewportTitlebar --or (not IsHoveringViewport and imgui.windowhovered("a | b"))
	local render, open = imgui.begin(Project.Name .. "###Viewport", if canInteract then "nb" else "ni | nb", true)

	if not open then
		task.defer(changeStageTo, "SelectProject" :: "SelectProject")
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
	ViewportRenderPosition = vector.create(w.x, w.y)
	ViewportRenderSize = vector.create(ws.x, ws.y)

	imgui.setviewportdockspace(0, 19, ews.x, ews.y - (if EditorPreferences.ToolsOpen.BottomBar then 54 else 19))
	PlayerInput:SetViewportInputRect(ViewportRenderPosition, ViewportRenderSize)

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

function Stages.SelectProject_Enter(DeltaTime)
	if not fs.isfile(EDITOR_RECENT_PROJECTS_PATH) then
		fs.write(EDITOR_RECENT_PROJECTS_PATH, "[]")
	end

	local rpstr, err = fs.read(EDITOR_RECENT_PROJECTS_PATH)

	if not rpstr then
		print(`Couldn't read {EDITOR_RECENT_PROJECTS_PATH}: {err}`)
		return
	end

	local rpjson = json.parse(rpstr)
	assert(typeof(rpjson) == "table")

	RecentProjects = rpjson :: typeof(RecentProjects)
	SavedProjectsCache = nil

	PluginManager.Shutdown()
	fs.setunqualifiedroot("")
end

function Stages.SelectProject_Leave(DeltaTime)
	PluginManager.Load()

	if EditorPreferences.ClearOutputOnProjectOpen then
		Log = {}
	end
end

local SelectorHoveringOver: number? = nil
local ProjectContextMenuTarget = 0

local function renderProjectSelectorTiles(Projects: { string })
	local cpos = imgui.cursorposition()
	local startX = cpos.x

	local windowSizeX = Renderer.Resolution.x
	local lastnl = #Projects
	local hoveringOverWip = nil

	for i = #Projects, 1, -1 do
		local proj = Projects[i]
		local clicked = false
		local hovered = false
		cpos = imgui.cursorposition()

		imgui.dummy(300, 300)
		imgui.setcursorposition(cpos.x, cpos.y)

		if SelectorHoveringOver == i then
			imgui.pushstylecolor(5, { 1, 1, 1, 1 })

			imgui.setcursorposition(cpos.x - 10, cpos.y - 10)
			imgui.beginchild(Project.Name .. "_Selection", 270, 280, "b", "ni")
			imgui.endchild()
			imgui.setcursorposition(cpos.x, cpos.y)

			imgui.popstylecolor()

			if PlayerInput:IsMouseButtonPressed(Enum.MouseButton.Right) then
				imgui.openpopup("ProjectContextMenu")
				ProjectContextMenuTarget = i
			end
		end

		imgui.image(`@cwd/projects/{ proj }/icon.png`, vector.create(250, 250))
		clicked, hovered = imgui.itemclicked(), imgui.itemhovered()
		imgui.setcursorposition(cpos.x, cpos.y + 250)
		imgui.text(proj)
		clicked, hovered = clicked or imgui.itemclicked(), hovered or imgui.itemhovered()

		if clicked then
			task.defer(openProject, proj)
		end

		if hovered then
			hoveringOverWip = i
		end

		if (lastnl - i) * 350 + 170 + 250 + 75 + startX > windowSizeX then
			-- new row
			cpos = vector.create(imgui.cursorposition().x, cpos.y + 270)
			imgui.setcursorposition(startX, cpos.y + 10)
			lastnl = i - 1
		else
			-- same row
			imgui.setcursorposition(cpos.x + 275 - 15, cpos.y)
			imgui.dummy(0, 0)
			imgui.sameline()
		end
	end

	SelectorHoveringOver = hoveringOverWip
end

local function renderRenderProjects()
	imgui.text("Recent")
	imgui.newline()

	local cpos = imgui.cursorposition()
	imgui.setcursorposition(cpos.x, cpos.y + 10)

	cpos = imgui.cursorposition()
	imgui.setcursorposition(cpos.x + 10, cpos.y)

	renderProjectSelectorTiles(RecentProjects)

	if imgui.beginpopup("ProjectContextMenu") then
		if imgui.menuitem("Remove from Recents") then
			table.remove(RecentProjects, ProjectContextMenuTarget)
			-- TODO: fix cast
			fs.write(EDITOR_RECENT_PROJECTS_PATH, json.encode(RecentProjects :: JsonSerializable))
		end

		imgui.endpopup()
	end
end

local function renderSavedProjects()
	if not SavedProjectsCache then
		SavedProjectsCache = {}
		assert(SavedProjectsCache)

		for path, type in fs.listdir("@cwd/projects") do
			if type == "d" and fs.isfile(path .. "/phoenix.conf") then
				local conf = json.parse(assert(fs.read(path .. "/phoenix.conf")))
				assert(typeof(conf) == "table")
				local title = conf["GameTitle" :: string]
				assert(typeof(title == "string"))

				table.insert(SavedProjectsCache, title)
			end
		end
	end

	imgui.text("All Projects")
	imgui.newline()

	local cpos = imgui.cursorposition()
	imgui.setcursorposition(cpos.x, cpos.y + 10)

	cpos = imgui.cursorposition()
	imgui.setcursorposition(cpos.x + 10, cpos.y)

	renderProjectSelectorTiles(assert(SavedProjectsCache))
	
	if imgui.beginpopup("ProjectContextMenu") then
		if SavedProjectsCache and imgui.menuitem("Delete") then
			local name = SavedProjectsCache[ProjectContextMenuTarget]

			local buttonSelection = Engine:ShowMessageBox(
				"Are you sure?",
				`Are you sure you want to delete '{ name }'? This cannot be undone.`,
				"yesno",
				"warning"
			)

			if buttonSelection == 1 then
				fs.remove(`@cwd/projects/{ name }`)

				AssetManager:UnloadTexture(`@cwd/projects/{ name }/icon.png`)
				table.remove(RecentProjects, table.find(RecentProjects, name))
				saveRecents()

				SavedProjectsCache = nil
			end
		end
	
		imgui.endpopup()
	end
end

local ProjectCategories: { { Name: string, Icon: string, Render: (number) -> (), OnSwitchTo: (()->())? } } = {
	{
		Name = "Recent",
		Icon = "@etextures/editor-icons/recent.png",
		Render = renderRenderProjects
	},
	{
		Name = "Saved",
		Icon = "@etextures/editor-icons/open.png",
		Render = renderSavedProjects,
		OnSwitchTo = function()
			SavedProjectsCache = nil
		end
	}
}

local SelectedProjectsCategory = 1
local PrevSelectedProjectsCategory = 0

function Stages.SelectProject(DeltaTime: any) -- TODO type error when annotating `DeltaTime` as `number`
	assert(typeof(DeltaTime) == "number")

	if imgui.beginfullscreen("Start Page", 0, 18) then
		local startPos = imgui.cursorposition()

		if imgui.beginchild("SidePanel", 165, 0, "b") then
			local cpos = imgui.cursorposition()
			imgui.setcursorposition(cpos.x - 4, cpos.y)

			if imgui.imagebutton("+", "@etextures/editor-icons/new.png", vector.create(150, 150)) then
				task.defer(createNewProject)
			end
			imgui.setitemtooltip("Create new Project")

			cpos = imgui.cursorposition()

			for i, cat in ProjectCategories do
				if SelectedProjectsCategory == i then
					imgui.setcursorposition(cpos.x - 5, cpos.y - 2)
					imgui.pushstylecolor(5, { 1, 1, 1, 1 })

					imgui.beginchild("Selection", 160, 160, "b", "ni")
					imgui.endchild()

					imgui.popstylecolor()
				end

				imgui.setcursorposition(cpos.x - 4, cpos.y)

				if imgui.imagebutton(cat.Name, cat.Icon, vector.create(150, 150)) then
					SelectedProjectsCategory = i
				end
				imgui.setitemtooltip(cat.Name)

				cpos = imgui.cursorposition()
			end

			if PrevSelectedProjectsCategory ~= SelectedProjectsCategory then
				local onSwitchTo = ProjectCategories[SelectedProjectsCategory].OnSwitchTo
				if onSwitchTo then
					onSwitchTo()
				end
			end
		end
		imgui.endchild()

		imgui.setcursorposition(startPos.x + 170, startPos.y)

		imgui.beginchild("MainSection", 0, 0, "b")

		ProjectCategories[SelectedProjectsCategory].Render(startPos.x)

		imgui.dummy()
		imgui.endchild()
	end
	imgui.endw()

	renderMainMenuBar(DeltaTime)
end

function Stages.ProjectEditing_Enter(PreviousStage: Stage)
	assert(Project.DataModel)
	assert(Project.Workspace)

	DeveloperTools:SetExplorerRoot(Project.DataModel)
	DeveloperTools:SetExplorerSelections({})
	Physics.Simulating = false
	workspace.SceneCamera = Project.Workspace.SceneCamera
	workspace.SceneCamera.UseSimpleController = true

	if PreviousStage == "SelectProject" then
		for icon, fstype in fs.listdir("@etextures/editor-icons") do
			if fstype == "f" and icon:find(".png") then
				AssetManager:QueueLoadTexture(icon, false, true)
			end
		end

		for _, reimportable in Collections:GetTagged(REIMPORTABLE_MODEL) :: { GameObject & Model & Transform } do
			if not reimportable:HasComponent("Model") or not reimportable:HasComponent("Transform") then
				continue
			end

			watchModelForReimporting(reimportable)
		end

		for path, watcher in ReimportWatchers do
			assert(watcher)

			if ModelModifiedTimes[path] and ModelModifiedTimes[path] < fs.lastwritten(path) then
				print(`Re-importing { path }`)
				local imported = AssetManager:ImportModel(path)
				assert(imported, "Re-import failed on Project open")

				local action = History:TryBeginAction("Auto re-import on Project open")

				for _, model in watcher.Models :: { GameObject & Model } do
					-- preserve tags, name, parent, object ID, treelinks

					for _, child in model:GetChildren() do
						child.Parent = nil -- Allow undo
					end

					for _, newChild in imported:GetChildren() do
						newChild:Duplicate().Parent = model
					end
				end

				if action then
					History:FinishAction(action)
				end

				ModelModifiedTimes[path] = fs.lastwritten(path)
			end
		end
	end
end

local ShowedAutosaveDelayedMessage = false

function Stages.ProjectEditing(DeltaTime: any)
	assert(typeof(DeltaTime) == "number")

	renderMainMenuBar(DeltaTime)
	renderViewport(DeltaTime)

	if (game.Time - LastAutosaved) / 60 > EditorPreferences.AutosaveInterval then
		if not History.CurrentActionName then
			saveProject()
			print("Autosaved")
			LastAutosaved = game.Time
			ShowedAutosaveDelayedMessage = false

		elseif not ShowedAutosaveDelayedMessage then
			setErrorNotification(`Autosave has been delayed until the { History.CurrentActionName } action finishes`)
			ShowedAutosaveDelayedMessage = true
		end
	end
end

function Stages.ProjectEditing_Leave(NextStage)
	if NextStage == "ProjectPlaying" then
		if EditorPreferences.ClearOutputOnPlaytest then
			Log = {}
		end

		return
	else
		for _, watcher in ReimportWatchers do
			assert(watcher)
			watcher.Disconnect()
		end

		ReimportWatchers = {}
	end
	
	DeveloperTools:SetExplorerRoot(game)
	DeveloperTools:SetExplorerSelections({})

	assert(Project.DataModel)
	if not EditorPreferences.ClosingProjectSaveRequiresConfirmation
		or Engine:ShowMessageBox("Save?", "Save datamodel before closing the current project?", "yesno", "question", 1) == 1
	then
		saveProject()
	end

	Project.DataModel:Destroy()
	Project.Workspace = nil
	Project.Name = "Should not be visible"
	Project.IsTempPath = true
	Project.DataModel = nil

	Engine:SaveConfig()
	Engine:CloseVM(GameEditingVM)
end

function Stages.ProjectPlaying_Enter(DeltaTime: any)
	PlayerInput.Cursor = Enum.Cursor.NotAllowed

	assert(Project)
	restoreState = Project.DataModel

	assert(Project.DataModel)
	Project.DataModel = Project.DataModel:Duplicate()

	DeveloperTools:SetExplorerRoot(Project.DataModel)
	DeveloperTools:SetExplorerSelections({})
	Physics.Simulating = true

	Project.DataModel.VM = GameVM
	Engine:BindDataModel(Project.DataModel) -- Set the `game` and `workspace` variables to the correct values in game scripts
	Engine:CreateVM(GameVM)

	local projWp = Project.Workspace
	assert(projWp)

	if not projWp.SceneCamera then
		projWp.SceneCamera = GameObject.new("Camera")
		projWp.SceneCamera.Parent = projWp
		projWp.SceneCamera.Name = "Camera"
	end

	workspace.SceneCamera = projWp.SceneCamera

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

	Project.DataModel:Destroy()
	Project.DataModel = restoreState
	DeveloperTools:SetExplorerRoot(Project.DataModel)

	Engine:BindDataModel(game)
	Engine:CloseVM(GameVM)

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

function Stages.Update(dt)
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
		changeStageTo("SelectProject")

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

	watchModelForReimporting(Object :: GameObject & Model)
end)

Collections:GetTagRemovedSignal(REIMPORTABLE_MODEL):Connect(function(Object)
	if not Object:HasComponent("Model") or not Object:HasComponent("Transform") then
		return
	end

	local model = Object :: GameObject & Model
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

changeStageTo("SelectProject")

if table.find(Engine:GetCliArguments(), "-ed_ci") then
	if StageUpdateSignal then
		StageUpdateSignal:Disconnect()
		StageUpdateSignal = nil
	end

	task.defer(function()
		warn("Running Editor CI checks")

		task.delay(5, function()
			warn("Potential CI stall - Exiting")
			Engine:Exit(1)
		end)

		local success = xpcall(createNewProject_, function(err)
			warn(debug.traceback(err, 1))
		end)

		if not success then
			warn("Failed to create new project!")
			Engine:Exit(1)
			return
		end

		if CurrentStage ~= "ProjectEditing" then
			warn(`Stage is {CurrentStage} and not ProjectEditing`)
			Engine:Exit(1)
			return
		end

		warn("Editor CI checks done")
		CurrentStage = "SelectProject"
		Engine:Exit()
	end)
end
