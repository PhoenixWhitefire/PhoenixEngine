-- ed.luau, 14/09/2025
local HistoryService: HistoryService = game:GetService("HistoryService") :: any
local InputService: InputService = game:GetService("InputService") :: any
local AssetService: AssetService = game:GetService("AssetService") :: any
local Engine: Engine = game:GetService("Engine") :: any

local FALLBACK_IMAGE = "@etextures/editor-icons/fallback.png"
local LOGO_IMAGE = "@etextures/logo.png"

local PluginManager = require("./PluginManager.luau")
local HistoryView = require("./HistoryView.luau")
local CoreTools = require("./CoreTools.luau")
local Types = require("./Types.luau")
type Stage = Types.Stage

local CurrentStage: Stage = "None"
local StageUpdateSignal: EventConnection? = nil
local Stages: { [string]: ((number) -> ()) & ((string) -> (any)) } = {}

local EditorCam = assert(workspace.SceneCamera)
local EditorTl = GameObject.new("TreeLink")

local Project: Types.Project = {
	DataModel = nil,
	Workspace = nil,
	Name = "Should not be visible",
	IsTempPath = false
}

local RecentProjects: { string } = {}
local EditorPreferences = {
	AutoLaunchCodeEditorOnProjectOpen = true,
	ClearOutputOnProjectOpen = true,
	ClearOutputOnPlaytest = true,
	CodeEditorPath = "code",
	UseLightTheme = false
}

local EDITOR_RECENT_PROJECTS_PATH = "./editor/recent-projects.json"

local GameEditingVM = "GameEditingVM"
local GameVM = "GameVM"

EditorCam.UseSimpleController = true
EditorTl.Parent = workspace
EditorTl.Scripting = false
Engine.PhysicsTimescale = 0

local ToolOrder = {
	"Tool_Explorer",
	"Tool_Properties"
}

local BottomBarEnabled = conf.get("Ed_Tool_BottomBarEnabled") or false
local OutputEnabled = conf.get("Ed_Tool_OutputEnabled") or false
assert(typeof(BottomBarEnabled) == "boolean")
assert(typeof(OutputEnabled) == "boolean")

for _, tool in Engine:GetToolNames() do
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

HistoryService:EnableRecording()

fs.definealias("editres", "@cwd/resources")
fs.definealias("etextures", "@editres/textures")

if not fs.isdirectory("@cwd/editor") then
	fs.mkdir("@cwd/editor")
end

if not fs.isdirectory("@cwd/projects") then
	fs.mkdir("@cwd/projects")
end

local function setErrorNotification(Error)
	ErrorMessage = Error
	ErrorMessageTimeRemaining = 2
	print(Error)
end

local function onError(err)
	local errorMessage = `An error occurred: { debug.traceback(err, 2) }`
	if Project.DataModel then
		errorMessage ..= "\n\nThe Editor will perform a save to the file 'resources/scenes/autosave.world', please ensure nothing important is stored at that location before proceeding."
	end

	print(errorMessage)
	Engine:ShowMessageBox("Editor Error", errorMessage, "ok", "error")

	if Project.DataModel then
		scene.save({ Project.DataModel }, "scenes/autosave.world")
	end
end

local changeStageTo: (NewStage: Stage) -> ()

local function handleUndoRedoKeybinds()
	local ctrlKeyDown = InputService:IsKeyPressed(Enum.Key.LeftControl)
	local zKeyDown = InputService:IsKeyPressed(Enum.Key.Z)
	local yKeyDown = InputService:IsKeyPressed(Enum.Key.Y)

	if ctrlKeyDown and zKeyDown then
		if not WasHoldingCtrlKeyDown or not WasHoldingZKeyDown then
			local success: boolean, error: string? = (pcall::any)(function()
				HistoryService:Undo()
			end)

			if not success then
				assert(error)

				local realError = error:sub(error:find(":") or 1, #error)
				realError = realError:sub((realError:find(" ") or 0) + 1, #realError)

				setErrorNotification(realError)
			end
		end
	end

	if ctrlKeyDown and yKeyDown then
		if not WasHoldingCtrlKeyDown or not WasHoldingYKeyDown then
			local success: boolean, error: string? = (pcall::any)(function()
				HistoryService:Redo()
			end)

			if not success then
				assert(error)

				local realError = error:sub(error:find(":") or 1, #error)
				realError = realError:sub((realError:find(" ") or 0) + 1, #realError)

				setErrorNotification(realError)
			end
		end
	end

	WasHoldingCtrlKeyDown = ctrlKeyDown
	WasHoldingZKeyDown = zKeyDown
	WasHoldingYKeyDown = yKeyDown
end

local function editorFrame(DeltaTime)
	MouseClick = (not WasHoldingDownM1) and InputService:IsMouseButtonPressed(Enum.MouseButton.Left)
	local f5 = InputService:IsKeyPressed(Enum.Key.F5)

	if not HistoryService.IsRecordingAction then
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

	CoreTools.Update(Project)
	Stages[CurrentStage](DeltaTime)

	WasHoldingDownM1 = InputService:IsMouseButtonPressed(Enum.MouseButton.Left)

	if ErrorMessageTimeRemaining > 0 then
		imgui.settooltip(ErrorMessage)
		ErrorMessageTimeRemaining -= DeltaTime
	end
end

changeStageTo = function(NewStage: Stage)
	if StageUpdateSignal then
		StageUpdateSignal:Disconnect()
	end
	
	assert(Stages[NewStage])
	
	if Stages[CurrentStage .. "_Leave"] then
		Stages[CurrentStage .. "_Leave"](NewStage)
	end
	
	local targetDm = game
	if Stages[NewStage .. "_Enter"] then
		targetDm = Stages[NewStage .. "_Enter"](CurrentStage) or targetDm
	end

	PluginManager.OnEditorStageChanged(CurrentStage, NewStage)
	CoreTools.OnEditorStageChanged(NewStage)
	
	CurrentStage = NewStage
	StageUpdateSignal = targetDm.OnFrameBegin:Connect(function(DeltaTime)
		local s, e = xpcall(function(dt)
			editorFrame(dt)
		end, onError, DeltaTime)

		if not s then
			print(`editorFrame error: {e}`)
		end
	end)
end

local function saveRecents()
	if not fs.write(EDITOR_RECENT_PROJECTS_PATH, json.encode(RecentProjects)) then
		print("Failed to save the Recent Projects list")
	end
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

local function saveProject()
	scene.save({ assert(Project.DataModel) }, "@projres/scenes/root.world")
end

local function openProject(ManualPath: string?)
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
				name = json.parse(conf).GameTitle
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

	fs.definealias("proj", "@cwd/projects/" .. assert(path))
	fs.definealias("projres", "@proj/resources")
	fs.makecwdaliasof("@proj/")

	assert(path)
	local objects, error = scene.load("@projres/scenes/root.world")

	if not objects then
		error = error or "Unknown error"
		Engine:ShowMessageBox("Project Error", `Couldn't open project '{path}': {error}`, "ok", "error")

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

	Project.DataModel.Name = Project.Name;

	Project.Workspace = Project.DataModel:FindChildWithComponent("Workspace") :: any

	if not Project.Workspace then
		Project.Workspace = GameObject.new("Workspace")
		Project.Workspace.SceneCamera = GameObject.new("Camera")
		Project.Workspace.SceneCamera.Parent = Project.Workspace

		print("Re-created Workspace")
	end

	if not Project.Workspace.SceneCamera then
		local cam = GameObject.new("Camera")
		cam.Parent = Project.Workspace
		Project.Workspace.SceneCamera = cam
	end

	EditorCam.Transform = Project.Workspace.SceneCamera.Transform
	EditorTl.Target = Project.Workspace

	HistoryService:ClearHistory()
	HistoryService.TargetDataModel = Project.DataModel

	changeStageTo("ProjectEditing")

	Engine:BindDataModel(Project.DataModel)
	Engine:CreateVM(GameEditingVM)
	Engine:BindDataModel(game)

	if EditorPreferences.AutoLaunchCodeEditorOnProjectOpen then
		print("Open code editor result: ", shellexec(`{ EditorPreferences.CodeEditorPath } "{ fs.cwd() }/projects/{ Project.Name }"`))
	end
end

local SavedProjectsCache: { string }? = nil

local function createNewProject()
	if not fs.isdirectory("@cwd/projects") then
		assert(fs.mkdir("@cwd/projects"))
	end

	local name = "New Project "
	local nameIndex = 1

	while fs.isdirectory("@cwd/projects/" .. name .. nameIndex) do
		nameIndex += 1
	end

	Project.Name = name .. nameIndex
	Project.IsTempPath = true

	local fullPath = "@cwd/projects/" .. assert(Project.Name)

	local success, createError = fs.mkdir(fullPath)
	if not success then
		Engine:ShowMessageBox("Failed to create project", `Failed to create project at '{fullPath}':\n{ assert(createError) }`)
		return
	end

	fs.definealias("proj", fullPath)
	fs.definealias("projres", "@proj/resources")

	assert(fs.write(fullPath .. "/phoenix.conf", json.encode({
		GameTitle = Project.Name,
		ResourcesDirectory = "resources/",
		RootScene = "scenes/root.world"
	})))

	assert(fs.mkdir("@projres"))
	assert(fs.mkdir("@projres/materials"))
	assert(fs.mkdir("@projres/scenes"))
	assert(fs.mkdir("@projres/sounds"))
	assert(fs.mkdir("@projres/shaders"))
	assert(fs.mkdir("@projres/scripts"))
	assert(fs.mkdir("@projres/textures"))
	assert(fs.mkdir("@projres/textures/materials"))

	for _, v in { "error", "worldUber", "worldUberTriProjected", "worldUberSkinned", "skybox", "boxframe", "particle", "postprocessing" } do
		if fs.isfile(`@editres/shaders/{v}.shp`) then
			assert(fs.copy(`@editres/shaders/{v}.shp`, `@projres/shaders/{v}.shp`))
		end
		if fs.isfile(`@editres/shaders/{v}.vert`) then
			assert(fs.copy(`@editres/shaders/{v}.vert`, `@projres/shaders/{v}.vert`))
		end
		if fs.isfile(`@editres/shaders/{v}.frag`) then
			assert(fs.copy(`@editres/shaders/{v}.frag`, `@projres/shaders/{v}.frag`))
		end
		if fs.isfile(`@editres/shaders/{v}.geom`) then
			assert(fs.copy(`@editres/shaders/{v}.geom`, `@projres/shaders/{v}.geom`))
		end
	end

	for _, v in { "brick", "error", "floortiles", "neon", "plastic", "smoothplastic", "wood", "woodplanks" } do
		assert(fs.copy(`@editres/materials/{v}.mtl`, `@projres/materials/{v}.mtl`))
	end

	assert(fs.copy("@editres/textures/materials", "@projres/textures/materials"))

	assert(fs.copy("@editres/textures/Sky1", "@projres/textures/Sky1"))

	assert(fs.copy(LOGO_IMAGE, "@proj/icon.png"))

	local dm = GameObject.new("DataModel")
	local projWp = GameObject.new("Workspace")
	local camera = GameObject.new("Camera")
	GameObject.new("DirectionalLight").Parent = workspace

	projWp.Parent = dm
	camera.Parent = projWp
	projWp.SceneCamera = camera
	Project.DataModel = dm

	saveProject()
	openProject(Project.Name)

	SavedProjectsCache = nil
end

local TabButtonHovering: string? = nil
local WipTabButtonHovering = TabButtonHovering
local WindowHovering = false

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

	if Lit then
		imgui.pushstylecolor(3, { 1, 1, 1, 0.05 })
	end

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

	if Lit then
		imgui.popstylecolor()
	end

	if Id == TabButtonHovering then
		imgui.popstylecolor()

		-- if we are hovering and mouseclick
		if Enabled and MouseClick then
			return true
		end
	end

	return if Enabled then clicked else false
end

local function whyCannotUndoRedo()
	return if HistoryService.IsRecordingAction
		then `You can't until the {assert(HistoryService.CurrentActionName)} Action finishes`
		else nil
end

local function homeTab(_: number)
	local startY = imgui.cursorposition().y

	local inPlaytest = CurrentStage == "ProjectPlaying"
	local currentActionTooltip = whyCannotUndoRedo()

	if inPlaytest then
		if tabButton("Stop", "@etextures/editor-icons/stop.png", not HistoryService.IsRecordingAction, nil, nil, currentActionTooltip) then
			changeStageTo("ProjectEditing")
		end
	else
		if tabButton("Start", "@etextures/editor-icons/start.png", not HistoryService.IsRecordingAction, nil, nil, currentActionTooltip) then
			changeStageTo("ProjectPlaying")
		end
	end

	imgui.sameline()

	if tabButton("Explorer to Project DM", "@etextures/editor-icons/ExplorerToProj.png") then
		assert(Project.DataModel)
		Engine:SetExplorerRoot(Project.DataModel)
	end

	imgui.sameline()
	
	if tabButton("Explorer to Editor DM", "@etextures/editor-icons/ExplorerToEdit.png") then
		Engine:SetExplorerRoot(game)
	end

	imgui.sameline()

	if tabButton("Import Model", "@etextures/editor-icons/Model.png") then
		task.defer(function()
			local paths = fs.promptopen(nil, { "*.gltf", "*.glb" }, "Models")

			if paths[1] then
				HistoryService:TryBeginAction("Import Model")

				local imported = AssetService:ImportModel(paths[1])

				-- TODO type of `imported` becomes `nil` after this clause for some reason
				--if not imported then
				--	print("Failed to import")
				--	HistoryService:DiscardCurrentAction()
				--	return
				--end

				assert(Project.Workspace)
				imported.Parent = Project.Workspace
				Engine:SetExplorerSelections({ imported })

				HistoryService:FinishCurrentAction()
			else
				print("No model selected")
			end
		end)
	end

	imgui.sameline()

	CoreTools.RenderControls(tabButton)

	imgui.sameline()

	local cx = imgui.cursorposition().x
	imgui.setcursorposition(cx, startY)

	if tabButton("History", "@etextures/editor-icons/history-viewer.png", true, HistoryView.Open) then
		HistoryView.Open = not HistoryView.Open
	end
end

local function toolsTab(_: number)
	for _, tool in ToolOrder do
		local name = tool:sub(#("Tool_") + 1, #tool)
		local enabled = Engine:IsToolEnabled(tool)

		if tabButton(name, `@etextures/editor-icons/{name}.png`, true, enabled) then
			Engine:SetToolEnabled(tool, not enabled)
			conf.save()
		end

		imgui.sameline()
	end

	if tabButton("Bottom Bar", `@etextures/editor-icons/command-bar.png`, true, BottomBarEnabled) then
		BottomBarEnabled = not BottomBarEnabled
		conf.set("Ed_Tool_BottomBarEnabled", BottomBarEnabled)
		conf.save()
	end

	imgui.sameline()

	if tabButton("Output", `@etextures/editor-icons/command-bar.png`, true, OutputEnabled) then
		OutputEnabled = not OutputEnabled
		conf.set("Ed_Tool_OutputEnabled", OutputEnabled)
		conf.save()
	end
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
local RibbonPopupDistance = 12

local function exportProject()
	saveProject()

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

	assert(fs.mkdir(exportTo))
	assert(fs.copy("@proj", exportTo))

	local execPath = Engine:GetCliArguments()[1]
	assert(fs.copy(execPath, `{exportTo}/{Project.Name}{if execPath:find(".exe") then ".exe" else ""}`))

	if execPath:find("x64/Debug") and execPath:find(".exe") then
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

local function renderMainMenuBar(dt: number)
	if not imgui.beginmainmenubar() then
		return
	end
	local mainMenuCursorY = InputService:GetCursorPosition().y
	HoveringOverMenuBar = mainMenuCursorY < RibbonPopupDistance

	local openAboutWindow = false
	local openSettingsWindow = false
	
	if imgui.beginmenu("File") then
		if CurrentStage == "ProjectEditing" then
			local undoRedoDisabledReason = whyCannotUndoRedo()

			if imgui.menuitem("Undo", HistoryService:CanUndo()) then
				HistoryService:Undo()
			end
			if undoRedoDisabledReason then
				imgui.setitemtooltip(undoRedoDisabledReason)
			end

			if imgui.menuitem("Redo", HistoryService:CanRedo()) then
				HistoryService:Redo()
			end
			if undoRedoDisabledReason then
				imgui.setitemtooltip(undoRedoDisabledReason)
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

								assert(fs.rename("@cwd/projects/" .. Project.Name, newProjectName))
								fs.definealias("proj", "@cwd/projects/" .. newProjectName)

								Project.Name = newProjectName

								local conf = json.parse(assert(fs.read("@proj/phoenix.conf")))
								conf.GameTitle = newProjectName

								assert(fs.write(
									"@proj/phoenix.conf",
									json.encode(conf)
								))

								table.remove(RecentProjects, table.find(RecentProjects, prevName))
								saveToRecents(newProjectName)
							else
								newProjectName = Project.Name
							end
						end

						imgui.text("Project icon:")

						if imgui.imagebutton("Icon", "@proj/icon.png", vector.create(150, 150)) then
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
										assert(fs.remove("@proj/icon.png"))
										assert(fs.copy(path, "@proj/icon.png"))
										Engine:UnloadTexture("@proj/icon.png") -- force the texture to be reloaded
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

			if imgui.menuitem("Export") then
				task.defer(exportProject)
			end

			if imgui.menuitem("Close") then
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

		if imgui.menuitem("Settings") then
			openSettingsWindow = true
		end
		
		imgui.endmenu()
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

		if (HoveringOverMenuBar or HoveringOverRibbonTab or TabButtonHovering ~= nil or WindowHovering) and not InputService:IsMouseButtonPressed(Enum.MouseButton.Right) then
			local windowWidth = Engine.WindowSize.x

			local TAB_HEIGHT = 175

			imgui.setnextwindowposition(0, 16)
			imgui.setnextwindowsize(windowWidth, TAB_HEIGHT)

			imgui.begin("RibbonTabContents", "nt | nr | nm | nc | ns | h")
			HoveringOverRibbonTab = imgui.windowhovered() or imgui.anyitemactive()
			RibbonTabs[CurrentRibbonTab].Renderer(dt)
			imgui.endw()

			RibbonPopupDistance = TAB_HEIGHT -- fixes ribbon closing when enabling tools
		else
			RibbonPopupDistance = 12
		end

		TabButtonHovering = WipTabButtonHovering
	end

	imgui.endmainmenubar()

	if openAboutWindow then
		imgui.openpopup("About")

		local ws = Engine.WindowSize

		local ABOUT_SIZE_X, ABOUT_SIZE_Y = 375, 280

		imgui.setnextwindowposition((ws.x - ABOUT_SIZE_X) / 2, (ws.y - ABOUT_SIZE_Y) / 2);
		imgui.setnextwindowsize(ABOUT_SIZE_X, ABOUT_SIZE_Y);
	end

	if imgui.beginpopupmodal("About", true) then
		local cxavail = imgui.contentregionavail().x
		local cursor = imgui.cursorposition()
		local cursory = cursor.y

		local LOGO_SIZE = 150

		imgui.setcursorposition(getCenteredX(cursor.x, cxavail, LOGO_SIZE), cursory)
		imgui.image(LOGO_IMAGE, vector.create(LOGO_SIZE, LOGO_SIZE))

		cursory += 160

		local tts = imgui.textsize("Phoenix Engine Editor")
		local vtsx = imgui.textsize(Engine.Version).x
		local ctsx = imgui.textsize(Engine.CommitHash:sub(1, 8)).x
		local utsx = imgui.textsize(GITHUB_URL).x

		imgui.setcursorposition(getCenteredX(cursor.x, cxavail, tts.x), cursory)
		imgui.text("Phoenix Engine Editor")
		cursory += tts.y + 2

		imgui.setcursorposition(getCenteredX(cursor.x, cxavail, vtsx), cursory)
		imgui.text(Engine.Version)
		cursory += tts.y + 15

		imgui.setcursorposition(getCenteredX(cursor.x, cxavail, utsx), cursory)
		imgui.urllink(GITHUB_URL)
		cursory += tts.y + 2

		imgui.setcursorposition(getCenteredX(cursor.x, cxavail, ctsx), cursory)
		imgui.urllink(Engine.CommitHash:sub(1, 8), `https://github.com/PhoenixWhitefire/PhoenixEngine/commit/{ Engine.CommitHash }`)

		imgui.endpopup()
	end

	if openSettingsWindow then
		imgui.openpopup("Settings")

		local SETTINGS_SIZE_X, SETTINGS_SIZE_Y = 420, 140

		local ws = Engine.WindowSize

		imgui.setnextwindowposition((ws.x - SETTINGS_SIZE_X) / 2, (ws.y - SETTINGS_SIZE_Y) / 2);
		imgui.setnextwindowsize(SETTINGS_SIZE_X, SETTINGS_SIZE_Y);
	end

	if imgui.beginpopupmodal("Settings", true) then
		local lightMode = imgui.checkbox("Light", EditorPreferences.UseLightTheme)
		if lightMode ~= EditorPreferences.UseLightTheme then
			imgui.stylecolors(if lightMode then "l" else "d")
		end
		EditorPreferences.UseLightTheme = lightMode

		EditorPreferences.CodeEditorPath = imgui.inputstring("Code Editor path", EditorPreferences.CodeEditorPath)
		EditorPreferences.AutoLaunchCodeEditorOnProjectOpen = imgui.checkbox("Launch code editor on project open", EditorPreferences.AutoLaunchCodeEditorOnProjectOpen)

		EditorPreferences.ClearOutputOnProjectOpen = imgui.checkbox("Clear Output on Project open", EditorPreferences.ClearOutputOnProjectOpen)
		EditorPreferences.ClearOutputOnPlaytest = imgui.checkbox("Clear Output on Playtest", EditorPreferences.ClearOutputOnPlaytest)

		if imgui.button("Save") then
			assert(fs.write("./editor/settings.json", json.encode(EditorPreferences)))
		end

		if imgui.treenode("##") then
			imgui.urllink("Configure spyware settings", SPYWARE_SETTINGS_URL)
			imgui.treepop()
		end

		imgui.endpopup()
	end
end

local savedEditorPreferences = if fs.isfile("./editor/settings.json") then json.parse(assert(fs.read("./editor/settings.json"))) else EditorPreferences

for k, v in EditorPreferences do
	if savedEditorPreferences[k] == nil then
		savedEditorPreferences[k] = v
	end
end

EditorPreferences = savedEditorPreferences

imgui.stylecolors(if EditorPreferences.UseLightTheme then 'l' else 'd')

local PrevSelections: { GameObject & Transform } = {}

local SelectionPart = GameObject.new("Mesh")
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
local Log: { { Closed: boolean, Message: { { Content: string, ExtraTags: string, CanInteract: boolean, Tooltip: string } }, Type: number, Time: string, Source: string? } } = {}
local NewLogMessage = false

local function canInteractWithLogSubMessage(ExtraTags: string): (boolean, string)
	local objectId = ExtraTags:match("Object:%d+")

	if objectId then
		local object = GameObject.fromId(assert(tonumber(objectId:sub(#("Object:") + 1))))

		return true, `Select { tostring(object) }`
	end

	local shortSrc = ExtraTags:match("TextDocument:.+,")

	if shortSrc then
		local lineMatch = ExtraTags:match("DocumentLine:%d+")

		return true, `Open { shortSrc:sub(#("TextDocument:") + 1, #shortSrc - 1) }{ if lineMatch then lineMatch:sub(#("DocumentLine:"), #lineMatch) else "" }`
	end

	return false, ""
end

local function interactWithLogSubMessage(ExtraTags: string)
	local objectId = ExtraTags:match("Object:%d+")

	if objectId then
		local object = GameObject.fromId(assert(tonumber(objectId:sub(#("Object:") + 1))))

		if object then
			Engine:SetExplorerSelections({ object })
		end
		return
	end

	local shortSrc = ExtraTags:match("TextDocument:.+,")

	if shortSrc then
		local lineMatch = ExtraTags:match("DocumentLine:%d+")

		local line = lineMatch and tonumber(lineMatch:sub(#("DocumentLine:") + 1))
		Engine:OpenTextDocument(shortSrc:sub(#("TextDocument:") + 1, #shortSrc - 1), line)

		return
	end

	warn(`Failed to figure out the interaction for a log message with tags '{ExtraTags}'`)
end

Engine.OnMessageLogged:Connect(function(Type: number, Message: string, ExtraTags: string)
	if Message == " &&" then
		Message = ""
	end

	if #Message >= 2 and Message:sub(-#("&&")) == "&&" then
		Message = Message:sub(1, #Message - 2)
	end

	local canInteract, tooltip = canInteractWithLogSubMessage(ExtraTags)

	local lastLog = Log[#Log]
	if lastLog and not lastLog.Closed then
		if Type == Enum.LogType.None then
			table.insert(lastLog.Message, {
				Content = Message,
				ExtraTags = ExtraTags,
				CanInteract = canInteract,
				Tooltip = tooltip
			})

			return
		else
			lastLog.Closed = true
		end
	end

	local source = ExtraTags:match("TextDocument:.+,") or ExtraTags:match("SourceChunkName:.+,")
	if source then
		source = source:sub(#("TextDocument:") + 1, #source - 1)
	end

	table.insert(Log, {
		Closed = false,
		Message = { { Content = Message, ExtraTags = ExtraTags, CanInteract = canInteract, Tooltip = tooltip } },
		Type = Type,
		Time = os.date("%H:%M:%S"),
		Source = source
	})

	NewLogMessage = true
end)

local function isMouseWithinViewport()
	local pos = InputService:GetCursorPosition()

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

local function renderViewport(DeltaTime: number)
	for index = 1, #PrevSelections do
		if not PrevSelections[index] then
			break
		end

		if not PrevSelections[index].Exists then
			PrevSelections[index] = PrevSelections[#PrevSelections]
			PrevSelections[#PrevSelections] = nil
			index -= 1
		end
	end

	if CurrentStage == "ProjectEditing" and Project.Workspace then
		local mouseVec = workspace:ScreenPointToVector(InputService:GetCursorPosition(), 1000)
		local rayresult = Project.Workspace:Raycast(workspace.SceneCamera.Transform.Position, mouseVec, { SelectionPart })

		if rayresult then
			SelectionPart.Transform = rayresult.Object.Transform
			SelectionPart.Size = rayresult.Object.Size + vector.create(0.05, 0.05, 0.05)
			SelectionPart.Parent = workspace

			if InputService:IsMouseButtonPressed(Enum.MouseButton.Left) and isMouseWithinViewport() then
				if InputService:IsKeyPressed(Enum.Key.LeftControl) then -- left control
					local sels = Engine:GetExplorerSelections()

					if not table.find(sels, rayresult.Object) then
						table.insert(sels, rayresult.Object :: GameObject)
					end

					Engine:SetExplorerSelections(sels)
				else
					Engine:SetExplorerSelections({ rayresult.Object })
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

	local newSelections = Engine:GetExplorerSelections()
	local selmap: { [number]: GameObject } = {}
	for i, sel_ in newSelections do
		if not sel_:HasComponent("Transform") then
			continue
		end

		local sel = (sel_ :: any) :: GameObject & Transform
		selmap[sel.ObjectId] = sel

		if not PrevSelections[sel.ObjectId] then
			local new: Mesh & Transform = SelectionPart:Duplicate() :: any
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
		if not sel or not sel.Exists or not sel:HasComponent("Transform") then
			box:Destroy()
			PrevSelections[id] = nil
		end
	end

	PluginManager.OnUpdate(DeltaTime, CurrentStage)
	CoreTools.Update(Project)

	if BottomBarEnabled then
		local w = Engine.WindowSize

		imgui.setnextwindowposition(0, w.y - 35)
		imgui.setnextwindowsize(w.x, 35)
		imgui.begin("Bottom Bar", "nt | nd | nr | ns")

		imgui.text("> ")
		imgui.sameline()
		CommandBarCommand = imgui.inputstring("##Command", CommandBarCommand)
		imgui.sameline()

		if imgui.button("Run") then
			local began = HistoryService:TryBeginAction("CommandBarCode")

			Engine:RunInVM(if CurrentStage == "ProjectEditing" then GameEditingVM else GameVM, CommandBarCommand, "=CommandBar")

			if began then
				HistoryService:FinishCurrentAction()
			end
		end
		imgui.sameline()

		imgui.text(`{ Project.Name } | { Engine.Framerate } FPS`)

		imgui.endw()
	end

	if OutputEnabled then
		imgui.begin("Output")

		local logMessageHoveringWip: number = -1
		local smid = 0

		for mi, message in Log do
			imgui.pushstylecolor(0, { 0.25, 0.25, 0.25, 1 })

			imgui.text(message.Time)

			imgui.popstylecolor()

			local LogColors = {
				[0] = { 1, 1, 1, 1 },
				[1] = { 1, 1, 1, 1 },
				[2] = { 255 / 255, 171 / 255, 61 / 255, 1 }, -- warning
				[3] = { 255 / 255, 61 / 255, 61 / 255, 1 }   -- error
			}

			local logColor = LogColors[message.Type]
			imgui.pushstylecolor(0, logColor)
			imgui.pushstylecolor(52, logColor)

			for smi, subMessage in message.Message do
				smid += 1

				if #subMessage.Content == 0 then
					continue
				end

				imgui.sameline()

				local context = subMessage.ExtraTags
				if #context == 0 then
					context = message.Message[1].ExtraTags
				end

				if (message.Source or subMessage.CanInteract) and LogMessageHovering == smid then
					if imgui.textlink(subMessage.Content) then
						interactWithLogSubMessage(context)
					end
				else
					imgui.text(subMessage.Content)
				end

				if imgui.itemhovered() then
					logMessageHoveringWip = smid
				end

				local tooltip = subMessage.Tooltip
				if #tooltip == 0 then
					tooltip = message.Message[1].Tooltip
				end

				if #tooltip == 0 then
					continue
				end

				imgui.setitemtooltip(tooltip)
			end

			imgui.popstylecolor()
			imgui.popstylecolor()
		end

		if NewLogMessage then
			imgui.scrollherey()
			NewLogMessage = false
		end

		if imgui.windowhovered() and InputService:IsMouseButtonPressed(Enum.MouseButton.Right) then
			imgui.openpopup("OutputContextMenu")
		end

		if imgui.beginpopup("OutputContextMenu") then
			if imgui.menuitem("Clear Output") then
				Log = {}
			end

			imgui.endpopup()
		end

		imgui.endw()

		LogMessageHovering = logMessageHoveringWip
	end

	local canInteract = IsHoveringViewportTitlebar or (not IsHoveringViewport and imgui.windowhovered("a | b"))
	
	imgui.begin(Project.Name .. "###Viewport", if canInteract then "nb" else "ni | nb")

	local cpos = InputService:GetCursorPosition()
	local w = imgui.windowposition()
	local ws = imgui.windowsize()

	local ews = Engine.WindowSize

	imgui.setviewportdockspace(0, 19, ews.x, ews.y - (if BottomBarEnabled then 54 else 19))
	Engine:SetViewport(vector.create(w.x, w.y), vector.create(ws.x, ws.y))

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
		assert(fs.write(EDITOR_RECENT_PROJECTS_PATH, "[]"))
	end

	local rpstr, err = fs.read(EDITOR_RECENT_PROJECTS_PATH)

	if not rpstr then
		print(`Couldn't read {EDITOR_RECENT_PROJECTS_PATH}: {err}`)
		return
	end

	RecentProjects = json.parse(rpstr)
	SavedProjectsCache = nil

	PluginManager.Shutdown()
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

	local windowSizeX = Engine.WindowSize.x
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

			if InputService:IsMouseButtonPressed(Enum.MouseButton.Right) then
				imgui.openpopup("ProjectContextMenu")
				ProjectContextMenuTarget = i
			end
		end

		imgui.image(`@cwd/projects/{proj}/icon.png`, vector.create(250, 250))
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
	local cpos = imgui.cursorposition()
	imgui.setcursorposition(cpos.x, cpos.y + 10)

	cpos = imgui.cursorposition()
	imgui.setcursorposition(cpos.x + 10, cpos.y)

	renderProjectSelectorTiles(RecentProjects)

	if imgui.beginpopup("ProjectContextMenu") then
		if imgui.menuitem("Remove from Recents") then
			table.remove(RecentProjects, ProjectContextMenuTarget)
			fs.write(EDITOR_RECENT_PROJECTS_PATH, json.encode(RecentProjects))
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
				assert(typeof(conf.GameTitle == "string"))

				table.insert(SavedProjectsCache, conf.GameTitle)
			end
		end
	end

	imgui.text("All Projects")
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
				`Are you sure you want to delete '{name}'? This cannot be undone.`,
				"yesno",
				"warning"
			)

			if buttonSelection == 1 then
				assert(fs.remove(`@cwd/projects/{name}`))

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
		Name = "Recents",
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

function Stages.ProjectEditing_Enter()
	assert(Project.DataModel)
	assert(Project.Workspace)

	Engine:SetExplorerRoot(Project.DataModel)
	Engine:SetExplorerSelections({})
	Engine.PhysicsTimescale = 0
	workspace.SceneCamera = Project.Workspace.SceneCamera
	workspace.SceneCamera.UseSimpleController = true
end

function Stages.ProjectEditing(DeltaTime: any)
	assert(typeof(DeltaTime) == "number")

	renderMainMenuBar(DeltaTime)
	renderViewport(DeltaTime)
	HistoryView.Render()
end

function Stages.ProjectEditing_Leave(NextStage)
	if NextStage == "ProjectPlaying" then
		if EditorPreferences.ClearOutputOnPlaytest then
			Log = {}
		end

		return
	end
	
	Engine:SetExplorerRoot(game)
	Engine:SetExplorerSelections({})

	assert(Project.DataModel)
	saveProject()

	Project.DataModel:Destroy()
	Project.Workspace = nil
	Project.Name = "Should not be visible"
	Project.IsTempPath = true
	Project.DataModel = nil

	conf.save()

	Engine:CloseVM(GameEditingVM)
end

local restoreState = nil

function Stages.ProjectPlaying_Enter(DeltaTime: any)
	assert(Project)
	restoreState = Project.DataModel

	assert(Project.DataModel)
	Project.DataModel = Project.DataModel:Duplicate() :: any

	Engine:SetExplorerRoot(Project.DataModel)
	Engine:SetExplorerSelections({})
	Engine.PhysicsTimescale = 1

	Engine:BindDataModel(Project.DataModel) -- Set the `game` and `workspace` variables to the correct values in game scripts
	Engine:CreateVM(GameVM)
	Engine:SetCurrentVM(GameVM)

	local projWp = Project.Workspace
	assert(projWp)

	if not projWp.SceneCamera then
		projWp.SceneCamera = GameObject.new("Camera")
		projWp.SceneCamera.Parent = projWp
	end

	workspace.SceneCamera = projWp.SceneCamera

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
	Engine:SetExplorerRoot(Project.DataModel)

	Engine:SetCurrentVM(_VMNAME)
	Engine:CloseVM(GameVM)
	Engine:BindDataModel(game)

	InputService.CursorMode = Enum.CursorMode.Normal
end

changeStageTo("SelectProject")
