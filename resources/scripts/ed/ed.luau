-- ed.luau, 14/09/2025
local FALLBACK_IMAGE = "@etextures/editor-icons/fallback.png"

local PluginManager = require("./PluginManager.luau")
local CoreTools = require("./CoreTools.luau")
local Types = require("./Types.luau")
type Stage = Types.Stage

local CurrentStage: Stage = "None"
local StageUpdateSignal: EventConnection? = nil
local Stages: { [string]: (any) -> () } = {}

local EditorCam = assert(workspace.SceneCamera)
local EditorTl = GameObject.new("TreeLink" :: "TreeLink")

local Project = {
	DataModel = nil :: (GameObject & DataModel)?,
	Workspace = nil :: (GameObject & Workspace)?,
	Name = "Should not be visible" :: string,
	IsTempPath = false :: boolean
}

local RecentProjects: { string } = {}

local EDITOR_RECENT_PROJECTS_PATH = "./editor/recent-projects.json"

EditorCam.UseSimpleController = true
EditorTl.Parent = workspace
EditorTl.Scripting = false
engine.physicstimescale(0)

local ToolOrder = {
	"Tool_Explorer",
	"Tool_Properties"
}

local BottomBarEnabled = conf.get("Ed_Tool_BottomBarEnabled")
assert(typeof(BottomBarEnabled) == "boolean" or typeof(BottomBarEnabled) == "nil")

for _, tool in engine.toolnames() do
	if not table.find(ToolOrder, tool) then
		table.insert(ToolOrder, tool)
	end
end

local WasHoldingDownM1 = false
local MouseClick = false

fs.definealias("editres", "@cwd/resources")
fs.definealias("etextures", "@editres/textures")

local function assert(cond: any, err: string?, ...)
	if not cond then
		local em = "Assertion failed: " .. (err or "")
		engine.showmessagebox("Editor error", debug.traceback(em, 1), "ok", "error")
		error(em)
	end

	return cond, err, ...
end

local function changeStageTo(NewStage: Stage)
	print(`Changing stage from { CurrentStage } to { NewStage }`)
	
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
		MouseClick = (not WasHoldingDownM1) and input.mousedown("l")
		CoreTools.Update(Project)

		Stages[CurrentStage](DeltaTime)

		WasHoldingDownM1 = input.mousedown("l")
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
		local open = imgui.beginpopup("Select Project")

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

		imgui.endpopup()

		if not atLeastOneProject then
			engine.showmessagebox("No projects", "You do not have any projects")
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
		engine.showmessagebox("Project Error", `Couldn't open project '{path}': {error}`, "ok", "error")

		return
	end

	for _, v in objects do
		if v:HasComponent("DataModel") then
			Project.DataModel = v :: any
			break
		end
	end

	if not Project.DataModel then
		Project.DataModel = GameObject.new("DataModel" :: "DataModel")
		print("Re-created DataModel")
	end

	Project.Workspace = Project.DataModel:FindChildWithComponent("Workspace") :: any

	if not Project.Workspace then
		Project.Workspace = GameObject.new("Workspace" :: "Workspace")
		Project.Workspace.SceneCamera = GameObject.new("Camera" :: "Camera")
		Project.Workspace.SceneCamera.Parent = Project.Workspace

		print("Re-created Workspace")
	end

	EditorCam.Transform = Project.Workspace.SceneCamera.Transform

	EditorTl.Target = Project.Workspace
	saveToRecents(path)

	Project.Name = path
	changeStageTo("ProjectEditing")
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
		engine.showmessagebox("Failed to create project", `Failed to create project at '{fullPath}':\n{ assert(createError) }`)
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
	assert(fs.mkdir("@projres/textures"))
	assert(fs.mkdir("@projres/textures/materials"))

	for _, v in { "error", "worldUber", "worldUberTriProjected", "worldUberSkinned", "skybox", "boxframe", "particle", "postprocessing" } do
		if fs.isfile(v .. ".shp") then
			fs.copy(`@editres/shaders/{v}.shp`, `@projres/shaders/{v}.shp`)
		end
		if fs.isfile(v .. ".vert") then
			fs.copy(`@editres/shaders/{v}.vert`, `@projres/shaders/{v}.vert`)
		end
		if fs.isfile(v .. ".frag") then
			fs.copy(`@editres/shaders/{v}.frag`, `@projres/shaders/{v}.frag`)
		end
		if fs.isfile(v .. ".geom") then
			fs.copy(`@editres/shaders/{v}.geom`, `@projres/shaders/{v}.geom`)
		end
	end

	for _, v in { "brick", "error", "floortiles", "neon", "plastic", "smoothplastic", "wood", "woodplanks" } do
		assert(fs.copy(`@editres/materials/{v}.mtl`, `@projres/materials/{v}.mtl`))
	end

	assert(fs.copy("@editres/textures/materials", "@projres/textures/materials"))

	assert(fs.copy("@editres/textures/Sky1", "@projres/textures/Sky1"))

	assert(fs.copy("@etextures/editor-icons/fallback.png", "@proj/icon.png"))

	local dm = GameObject.new("DataModel" :: "DataModel")
	local projWp = GameObject.new("Workspace" :: "Workspace")
	local camera = GameObject.new("Camera" :: "Camera")
	GameObject.new("DirectionalLight" :: "DirectionalLight").Parent = workspace

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

local function tabButton(Text: string, Icon: string, Enabled: boolean?, Lit: boolean?, Id: string?): boolean
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
	local cx, cy = imgui.cursorposition()
	imgui.setcursorposition(cx + 2, cy)

	imgui.image(Icon, { 100, 100 }, false, tint, false)
	clicked, hovered = imgui.itemclicked(), imgui.itemhovered()
	imgui.text(Text)
	clicked, hovered = clicked or imgui.itemclicked(), hovered or imgui.itemhovered()

	local windowHovered = imgui.windowhovered()

	imgui.endchild()

	if Enabled and (hovered or windowHovered) then
		WipTabButtonHovering = Id
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

local function homeTab(_: number)
	local inPlaytest = CurrentStage == "ProjectPlaying"

	if inPlaytest then
		if tabButton("Stop", "@etextures/editor-icons/stop.png") then
			changeStageTo("ProjectEditing")
		end
	else
		if tabButton("Start", "@etextures/editor-icons/start.png") then
			changeStageTo("ProjectPlaying")
		end
	end

	imgui.sameline()

	if tabButton("Explorer to Project DM", "@etextures/editor-icons/ExplorerToProj.png") then
		assert(Project.DataModel)
		engine.setexplorerroot(Project.DataModel)
	end

	imgui.sameline()
	
	if tabButton("Explorer to Editor DM", "@etextures/editor-icons/ExplorerToEdit.png") then
		engine.setexplorerroot(game)
	end

	imgui.sameline()

	if tabButton("Import Model", "@etextures/editor-icons/Model.png") then
		defer(function()
			local paths = fs.promptopen("models/", { "*.gltf", "*.glb" }, "Models")

			if paths[1] then
				local imported = model.import(paths[1])

				if not imported then
					print("Failed to import")
					return
				end

				assert(Project.Workspace)
				imported.Parent = Project.Workspace
				engine.setexplorerselections({ imported })
			else
				print("No model selected")
			end
		end)
	end

	imgui.sameline()

	CoreTools.RenderControls(tabButton)
end

local function toolsTab(_: number)
	for _, tool in ToolOrder do
		local name = tool:sub(#("Tool_") + 1, #tool)
		local enabled = engine.toolenabled(tool)

		if tabButton(name, `@etextures/editor-icons/{name}.png`, true, enabled) then
			engine.settoolenabled(tool, not enabled)
			conf.save()
		end

		imgui.sameline()
	end

	if tabButton("Bottom Bar", `@etextures/editor-icons/fallback.png`, true, BottomBarEnabled) then
		BottomBarEnabled = not BottomBarEnabled
		conf.set("Ed_Tool_BottomBarEnabled", BottomBarEnabled)
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

local IsLightMode = false
local CurrentRibbonTab: number = 1

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

		local cax, _ = imgui.getcontentregionavail()
		local cx, cy = imgui.cursorposition()
		imgui.setcursorposition(cx + cax - 50, cy)

		if imgui.button("Export") then
			print("Export button clicked")

			if fs.isdirectory(exportTo) then
				local chosenOption = engine.showmessagebox(
					"Export will overwrite",
					`The directory:\n\n{ exportTo }\n\nalready exists. Are you sure you want to overwrite it? This will erase all of it's content!`,
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

	local execPath = engine.args()[1]
	assert(fs.copy(execPath, `{exportTo}/{Project.Name}{if execPath:find(".exe") then ".exe" else ""}`))

	if execPath:find("x64/Debug") and execPath:find(".exe") then
		engine.showmessagebox(
			"Windows Debug build",
			"Windows Debug builds cannot be distributed without additional dependencies"
		)
	end

	print("Export done")
end

local function renderMainMenuBar(dt: number)
	if not imgui.beginmainmenubar() then
		return
	end
	local _, cursorY = input.mouseposition()
	HoveringOverMenuBar = cursorY < RibbonPopupDistance

	local openAboutWindow = false
	
	if imgui.beginmenu("File") then
		if imgui.menuitem("Save", CurrentStage == "ProjectEditing") then
			saveProject()
		end

		if CurrentStage == "ProjectEditing" then
			if imgui.menuitem("Project Settings") then
				defer(function()
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
							local buttonSelection = engine.showmessagebox("Rename project?", "Are you sure you want to rename the project?", "yesno")

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

						if imgui.imagebutton("Icon", "@proj/icon.png", { 150, 150 }) then
							local buttonSelection = engine.showmessagebox(
								"Change icon?",
								"Change the project icon? Note that the original icon file will be overwritten!",
								"yesno",
								"question"
							)

							if buttonSelection == 1 then
								defer(function()
									local path = fs.promptopen("@etextures/", { "*.png" , "*.jpg", "*.jpeg" }, "Images")[1]

									if path then
										assert(fs.remove("@proj/icon.png"))
										assert(fs.copy(path, "@proj/icon.png"))
										engine.unloadtexture("@proj/icon.png") -- force the texture to be reloaded
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
				defer(exportProject)
			end

			if imgui.menuitem("Close") then
				defer(changeStageTo, 0, "SelectProject" :: "SelectProject")
			end
		end

		if imgui.menuitem("Open", CurrentStage == "SelectProject") then
			SavedProjectsCache = nil
			defer(openProject)
		end

		if imgui.menuitem("New", CurrentStage == "SelectProject") then
			defer(createNewProject)
		end

		if imgui.menuitem("About") then
			openAboutWindow = true
		end

		local lightMode = imgui.checkbox("Light", IsLightMode)
		if lightMode ~= IsLightMode then
			imgui.stylecolors(if lightMode then "l" else "d")
		end
		IsLightMode = lightMode
		
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

		if (HoveringOverMenuBar or HoveringOverRibbonTab or TabButtonHovering ~= nil or WindowHovering) and not input.mousedown('r') then
			local windowWidth, _ = engine.windowsize()

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

		local wsx, wsy = engine.windowsize()

		imgui.setnextwindowposition((wsx - 375) / 2, (wsy - 240) / 2 - 100, "once");
		imgui.setnextwindowsize(375, 240, "once");
	end

	if imgui.beginpopupmodal("About", true) then
		local cxavail, _ = imgui.getcontentregionavail()
		local cursorx, cursory = imgui.cursorposition()

		imgui.setcursorposition(cursorx + cxavail / 2 - 150 / 2, cursory)

		imgui.image("@etextures/logo.png", { 150, 150 })

		local ttsx, ttsy = imgui.textsize("Phoenix Engine Editor")
		local utsx, _ = imgui.textsize(GITHUB_URL)

		imgui.setcursorposition(cursorx + (cxavail - ttsx) / 2, cursory + 160)
		imgui.text("Phoenix Engine Editor")

		imgui.setcursorposition(cursorx + (cxavail - utsx) / 2, cursory + 160 + ttsy + 10)
		imgui.urllink(GITHUB_URL)

		imgui.endpopup()
	end
end

local PrevSelections: { GameObject & Transform } = {}

local SelectionPart = GameObject.new("Mesh" :: "Mesh")
SelectionPart.Material = "boxframe"
SelectionPart.Tint = Color.new(166/255, 200/255, 1)
SelectionPart.PhysicsCollisions = false

local CommandBarCommand = ""
local IsHoveringViewportTitlebar = false
local IsHoveringViewport = false
local ViewportRight = 0
local ViewportLeft = 0
local ViewportTop = 0
local ViewportBottom = 0

local function isMouseWithinViewport()
	local mx, my = input.mouseposition()

	if mx >= ViewportLeft and mx <= ViewportRight then
		if my >= ViewportTop and my < ViewportBottom then
			IsHoveringViewport = true
		else
			IsHoveringViewport = false
		end
	else
		IsHoveringViewport = false
	end

	return IsHoveringViewport
end

local function renderViewport(DeltaTime: number)
	if CurrentStage == "ProjectEditing" and Project.Workspace then
		local mouseVec = Project.Workspace:ScreenPointToRay(input.mouseposition(), 100)
		local rayresult = Project.Workspace:Raycast(EditorCam.Transform.Position, mouseVec, { SelectionPart })

		if rayresult then
			SelectionPart.Transform = rayresult.Object.Transform
			SelectionPart.Size = rayresult.Object.Size
			SelectionPart.Parent = workspace

			if input.mousedown("l") and isMouseWithinViewport() then
				if input.keypressed(341) then -- left control
					local sels = engine.explorerselections()
					table.insert(sels, rayresult.Object :: GameObject)
					engine.setexplorerselections(sels)
				else
					engine.setexplorerselections({ rayresult.Object })
				end
			end
		else
			SelectionPart.Parent = nil
			--print(mouseVec)
		end
	else
		for _, v in PrevSelections do
			v:Destroy()
		end
		PrevSelections = {}
	end

	local newSelections = engine.explorerselections()
	local selmap: { [number]: GameObject } = {}
	for i, sel_ in newSelections do
		if not sel_:HasComponent("Transform") then
			continue
		end

		local sel = (sel_ :: any) :: GameObject & Transform
		selmap[sel.ObjectId] = sel

		if not PrevSelections[sel.ObjectId] then
			local new = SelectionPart:Duplicate()
			new.Transform, new.Size = sel.Transform, sel.Size
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
		local wx, wy = engine.windowsize()

		imgui.setnextwindowposition(0, wy - 35)
		imgui.setnextwindowsize(wx, 35)
		imgui.begin("Bottom Bar", "nt | nd | nr | ns")

		imgui.text("> ")
		imgui.sameline()
		CommandBarCommand = imgui.inputstring("##Command", CommandBarCommand)
		imgui.sameline()

		if imgui.button("Run") then
			local thread, error = loadthread(CommandBarCommand, "CommandBar")

			if not thread then
				print(`Failed to run command, error: {error}`)
			else
				coroutine.resume(thread)
			end
		end
		imgui.sameline()

		imgui.text(`{ Project.Name } | { engine.framerate() } FPS`)

		imgui.endw()
	end

	local canInteract = IsHoveringViewportTitlebar or (not IsHoveringViewport and imgui.windowhovered("a | b"))
	
	imgui.begin(Project.Name .. "###Viewport", if canInteract then "nb" else "ni | nb")

	local mx, my = input.mouseposition()
	local wx, wy = imgui.windowposition()
	local wsx, wsy = imgui.windowsize()

	local ewsx, ewsy = engine.windowsize()

	imgui.setviewportdockspace(0, 19, ewsx, ewsy - (if BottomBarEnabled then 54 else 19))
	engine.setviewport(wx, wy, wsx, wsy)

	ViewportLeft = wx - 15
	ViewportRight = wx + wsx + 15
	ViewportTop = wy
	ViewportBottom = wy + wsy

	if mx >= wx - 15 and mx <= wx + wsx + 15 then
		if my >= wy and my < wy + 16 then
			IsHoveringViewportTitlebar = true
		else
			IsHoveringViewportTitlebar = false
		end
	else
		IsHoveringViewportTitlebar = false
	end
	isMouseWithinViewport()

	imgui.setcursorposition(0, wsy - ewsy)

	imgui.image("!Framebuffer:Main", nil, true)

	imgui.endw()
	
end

if not fs.isdirectory("./editor") then
	fs.mkdir("./editor")
end

function Stages.SelectProject_Enter()
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

function Stages.SelectProject_Leave()
	PluginManager.Load()
end

local SelectorHoveringOver: number? = nil
local ProjectContextMenuTarget = 0

local function renderProjectSelectorTiles(Projects: { string })
	local cx, cy = imgui.cursorposition()
	local startX = cx

	local windowSizeX, _ = engine.windowsize()
	local lastnl = #Projects
	local hoveringOverWip = nil

	for i = #Projects, 1, -1 do
		local proj = Projects[i]
		local clicked = false
		local hovered = false
		cx, cy = imgui.cursorposition()

		imgui.dummy(300, 300)
		imgui.setcursorposition(cx, cy)

		if SelectorHoveringOver == i then
			imgui.pushstylecolor(5, { 1, 1, 1, 1 })

			imgui.setcursorposition(cx - 10, cy - 10)
			imgui.beginchild(Project.Name .. "_Selection", 270, 280, "b", "ni")
			imgui.endchild()
			imgui.setcursorposition(cx, cy)

			imgui.popstylecolor()

			if input.mousedown("r") then
				imgui.openpopup("ProjectContextMenu")
				ProjectContextMenuTarget = i
			end
		end

		imgui.image(`@cwd/projects/{proj}/icon.png`, { 250, 250 })
		clicked, hovered = imgui.itemclicked(), imgui.itemhovered()
		imgui.setcursorposition(cx, cy + 250)
		imgui.text(proj)
		clicked, hovered = clicked or imgui.itemclicked(), hovered or imgui.itemhovered()

		if clicked then
			defer(openProject, 0, proj)
		end

		if hovered then
			hoveringOverWip = i
		end

		if (lastnl - i) * 350 + 170 + 250 + 75 + startX > windowSizeX then
			-- new row
			_, cy = imgui.cursorposition()
			imgui.setcursorposition(startX, cy + 10)
			lastnl = i - 1
		else
			-- same row
			imgui.setcursorposition(cx + 275 - 15, cy)
			imgui.dummy(0, 0)
			imgui.sameline()
		end
	end

	SelectorHoveringOver = hoveringOverWip
end

local function renderRenderProjects()
	imgui.text("Recent")
	local cx, cy = imgui.cursorposition()
	imgui.setcursorposition(cx, cy + 10)

	cx, cy = imgui.cursorposition()
	imgui.setcursorposition(cx + 10, cy)

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
	local cx, cy = imgui.cursorposition()
	imgui.setcursorposition(cx, cy + 10)

	cx, cy = imgui.cursorposition()
	imgui.setcursorposition(cx + 10, cy)

	renderProjectSelectorTiles(assert(SavedProjectsCache))
	
	if imgui.beginpopup("ProjectContextMenu") then
		if SavedProjectsCache and imgui.menuitem("Delete") then
			local name = SavedProjectsCache[ProjectContextMenuTarget]

			local buttonSelection = engine.showmessagebox(
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

function Stages.SelectProject(DeltaTime: number)
	if imgui.beginfullscreen("Select Project", 0, 18) then
		local startPosX, startPosY = imgui.cursorposition()

		if imgui.beginchild("SidePanel", 165, 0, "b") then
			local cx, cy = imgui.cursorposition()
			imgui.setcursorposition(cx - 4, cy)

			if imgui.imagebutton("+", "@etextures/editor-icons/new.png", { 150, 150 }) then
				defer(createNewProject)
			end

			cx, cy = imgui.cursorposition()

			for i, cat in ProjectCategories do
				if SelectedProjectsCategory == i then
					imgui.setcursorposition(cx - 5, cy - 2)
					imgui.pushstylecolor(5, { 1, 1, 1, 1 })

					imgui.beginchild("Selection", 160, 160, "b", "ni")
					imgui.endchild()

					imgui.popstylecolor()
				end

				imgui.setcursorposition(cx - 4, cy)

				if imgui.imagebutton(cat.Name, cat.Icon, { 150, 150 }) then
					SelectedProjectsCategory = i
				end

				cx, cy = imgui.cursorposition()
			end

			if PrevSelectedProjectsCategory ~= SelectedProjectsCategory then
				local onSwitchTo = ProjectCategories[SelectedProjectsCategory].OnSwitchTo
				if onSwitchTo then
					onSwitchTo()
				end
			end
		end
		imgui.endchild()

		imgui.setcursorposition(startPosX + 170, startPosY)

		imgui.beginchild("MainSection", 0, 0, "b")

		ProjectCategories[SelectedProjectsCategory].Render(startPosX)

		imgui.dummy()
		imgui.endchild()
	end
	imgui.endw()
	
	renderMainMenuBar(DeltaTime)
end

function Stages.ProjectEditing_Enter(DeltaTime)
	assert(Project.DataModel)
	engine.setexplorerroot(Project.DataModel)
	engine.setexplorerselections({})
	engine.physicstimescale(0)

	local projWp = Project.Workspace

	if projWp and projWp.SceneCamera and projWp.SceneCamera:HasComponent("Camera") then
		projWp.SceneCamera.Transform = EditorCam.Transform
		EditorCam.FieldOfView = projWp.SceneCamera.FieldOfView
	end
end

function Stages.ProjectEditing(DeltaTime)
	local projWp = Project.Workspace

	-- sometimes this can get called after `ProjectEditing_Leave` or something somehow
	if projWp and projWp.SceneCamera and projWp.SceneCamera:HasComponent("Camera") then
		projWp.SceneCamera.Transform = EditorCam.Transform
		EditorCam.FieldOfView = projWp.SceneCamera.FieldOfView
	end

	renderMainMenuBar(DeltaTime)
	renderViewport(DeltaTime)
end

function Stages.ProjectEditing_Leave(NextStage)
	if NextStage == "ProjectPlaying" then
		return
	end
	
	engine.setexplorerroot(game)
	engine.setexplorerselections({})

	saveProject()

	Project.DataModel:Destroy()
	Project.Workspace = nil
	Project.Name = "Should not be visible"
	Project.IsTempPath = true
	Project.DataModel = nil

	conf.save()
end

local restoreState = nil

function Stages.ProjectPlaying_Enter()
	assert(Project)
	restoreState = Project.DataModel
	
	assert(Project.DataModel)
	Project.DataModel = Project.DataModel:Duplicate() :: any
	
	engine.setexplorerroot(Project.DataModel)
	engine.setexplorerselections({})
	engine.physicstimescale(1)

	engine.binddatamodel(Project.DataModel) -- Set the `game` and `workspace` variables to the correct values in game scripts
	engine.pushlvm("GameScripts")
	
	local projWp = Project.Workspace
	assert(projWp)

	if not projWp.SceneCamera then
		projWp.SceneCamera = GameObject.new("Camera" :: "Camera")
		projWp.SceneCamera.Parent = projWp
	end

	workspace.SceneCamera = projWp.SceneCamera
	
	return Project.DataModel -- Connect to the Project's datamodel `.OnFrameBegin`
end

function Stages.ProjectPlaying(DeltaTime)
	renderMainMenuBar(DeltaTime)
	renderViewport(DeltaTime)
end

function Stages.ProjectPlaying_Leave()
	assert(restoreState)
	assert(Project.DataModel)

	Project.DataModel:Destroy()
	Project.DataModel = restoreState
	engine.setexplorerroot(Project.DataModel)

	engine.poplvm()
	engine.binddatamodel(game)
	
	workspace.SceneCamera = EditorCam
end

changeStageTo("SelectProject")
