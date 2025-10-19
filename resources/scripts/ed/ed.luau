-- ed.luau, 14/09/2025
type Stage = "None" | "SelectProject" | "ProjectEditing" | "ProjectPlaying"

local FALLBACK_IMAGE = "textures/editor-icons/fallback.png"

local CurrentStage: Stage = "None"
local StageUpdateSignal: EventConnection? = nil
local Stages: { [string]: (any) -> () } = {}
local PluginManager = require("./PluginManager.luau")

local EditorCam = assert(workspace.SceneCamera)
local EditorTl = GameObject.new("TreeLink") :: GameObject & TreeLink

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

--  only want Explorer and Properties to be enabled by default
engine.settoolenabled("Tool_Explorer", true)
engine.settoolenabled("Tool_Properties", true)

local WasHoldingDownM1 = false
local MouseClick = false

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
	
	CurrentStage = NewStage
	StageUpdateSignal = targetDm.OnFrameBegin:Connect(function(DeltaTime)
		MouseClick = (not WasHoldingDownM1) and input.mousedown("l")

		Stages[CurrentStage](DeltaTime)

		WasHoldingDownM1 = input.mousedown("l")
	end)
end

local projDm: (GameObject & DataModel)? = nil
local projWp: (GameObject & Workspace)? = nil
local projCam: (GameObject & Camera)? = nil
local projectSavePath: string? = nil
local projectIcon: string? = nil

local function createNewProject()
	projDm = GameObject.new("DataModel") :: GameObject & DataModel
	projWp = GameObject.new("Workspace") :: GameObject & Workspace
	projCam = GameObject.new("Camera") :: GameObject & Camera

	projWp.Parent = projDm
	projCam.Parent = projWp
	projWp.SceneCamera = projCam
	EditorTl.Parent = workspace
	EditorTl.Target = projWp
	EditorTl.Scripting = false

	GameObject.new("DirectionalLight").Parent = projWp

	engine.setexplorerroot(projDm)
	
	projectSavePath = nil
	projectIcon = nil
	changeStageTo("ProjectEditing")
end

local recentProjects: { { Path: string, Icon: string? } } = {}

local function saveRecents()
	if not fs.write("recent-projects.json", json.encode(recentProjects)) then
		print("Failed to save the Recent Projects list")
	end
end

local function saveToRecents(Path: string)
	local oldIndex = nil

	for i, v in recentProjects do
		if v.Path == Path then
			oldIndex = i
			break
		end
	end

	if oldIndex then
		table.remove(recentProjects, oldIndex)
	end
	table.insert(recentProjects, { Path = Path, Icon = projectIcon })
	saveRecents()
end

local function openProject(ManualPath: string?)
	local path = ManualPath or fs.promptopen("scenes/", "*.world", "Worlds")[1]
	if not path then
		print("No project selected")
		return
	end

	for _, v in recentProjects do
		if v.Path == path then
			projectIcon = v.Icon
			break
		else
			projectIcon = FALLBACK_IMAGE
		end
	end

	local objects, error = scene.load(path)

	if not objects then
		error = error or "Unknown error"
		engine.showmessagebox("Project Error", `Couldn't open project '{path}': {error}`, "ok", "error")

		return
	end

	projDm = (assert(objects[1]) :: any) :: GameObject & DataModel
	projWp = assert(projDm.Workspace) :: GameObject & Workspace
	projCam = projWp.SceneCamera
	EditorTl.Target = projWp
	saveToRecents(path)

	projectSavePath = path
	changeStageTo("ProjectEditing")
end

local TabButtonHovering: string? = nil
local WipTabButtonHovering = TabButtonHovering
local WindowHovering = false

local function tabButton(Text: string, Icon: string, Enabled: boolean?, Lit: boolean?, Id: string?)
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

	imgui.image(Icon, { 100, 100 }, false, tint)
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
		if tabButton("Stop", "textures/editor-icons/stop.png") then
			changeStageTo("ProjectEditing")
		end
	else
		if tabButton("Start", "textures/editor-icons/start.png") then
			changeStageTo("ProjectPlaying")
		end
	end

	imgui.sameline()

	if tabButton("Explorer to Project DM", "textures/editor-icons/ExplorerToProj.png") then
		assert(projDm)
		engine.setexplorerroot(projDm)
	end

	imgui.sameline()
	
	if tabButton("Explorer to Editor DM", "textures/editor-icons/ExplorerToEdit.png") then
		engine.setexplorerroot(game)
	end

	imgui.sameline()

	if tabButton("Import Model", "textures/editor-icons/Model.png") then
		defer(function()
			local paths = fs.promptopen("models/", { "*.gltf", "*.glb" }, "Models")

			if paths[1] then
				local imported = model.import(paths[1])

				if not imported then
					print("Failed to import")
					return
				end

				imported.Parent = projWp
				engine.setexplorerselections({ imported })
			else
				print("No model selected")
			end
		end)
	end
end

local function toolsTab(_: number)
	for _, tool in ToolOrder do
		local name = tool:sub(#("Tool_") + 1, #tool)
		local enabled = engine.toolenabled(tool)

		if tabButton(name, `textures/editor-icons/{name}.png`, true, enabled) then
			engine.settoolenabled(tool, not enabled)
			conf.save()
		end

		imgui.sameline()
	end

	if tabButton("Bottom Bar", `textures/editor-icons/fallback.png`, true, BottomBarEnabled) then
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

local AboutWindowOpen = false
local IsLightMode = false
local CurrentRibbonTab: number = 1

local GITHUB_URL = "https://github.com/PhoenixWhitefire/PhoenixEngine"

local HoveringOverMenuBar = false
local HoveringOverRibbonTab = false
local RibbonPopupDistance = 24

local function renderMainMenuBar(dt: number)
	if not imgui.beginmainmenubar() then
		return
	end
	local _, cursorY = input.mouseposition()
	HoveringOverMenuBar = cursorY < RibbonPopupDistance
	
	if imgui.beginmenu("File") then
		if imgui.menuitem("Save", CurrentStage == "ProjectEditing") then
			imgui.endmenu()
			imgui.endmainmenubar()

			local path = projectSavePath or fs.promptsave("scenes/", "*.world", "Worlds")
			if not path then
				print("No path was selected for saving, cancelled")
				return
			end

			saveToRecents(path)
			assert(projDm)
			scene.save({ projDm }, path)

			return
		end

		if CurrentStage == "ProjectEditing" then
			if imgui.menuitem("Set icon") then
				imgui.endmenu()
				imgui.endmainmenubar()

				projectIcon = fs.promptopen("textures/", { "*.png" , "*.jpg", "*.jpeg" }, "Images")[1]

				return
			end

			if imgui.menuitem("Close") then
				imgui.endmenu()
				imgui.endmainmenubar()

				changeStageTo("SelectProject")

				return
			end
		end

		if imgui.menuitem("Open", CurrentStage == "SelectProject") then
			imgui.endmenu()
			imgui.endmainmenubar()

			openProject()
			return
		end

		if imgui.menuitem("New", CurrentStage == "SelectProject") then
			imgui.endmenu()
			imgui.endmainmenubar()
				
			createNewProject()
			return
		end

		if not AboutWindowOpen and imgui.menuitem("About") then
			AboutWindowOpen = true
			imgui.setnextwindowopen()
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

		if HoveringOverMenuBar or HoveringOverRibbonTab or TabButtonHovering ~= nil or WindowHovering then
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
			RibbonPopupDistance = 24
		end

		TabButtonHovering = WipTabButtonHovering
	end

	imgui.endmainmenubar()

	if AboutWindowOpen then
		imgui.setnextwindowfocus()

		if imgui.begin("About") then
			local cxavail, _ = imgui.getcontentregionavail()
			local cursorx, cursory = imgui.cursorposition()

			imgui.setcursorposition(cursorx + cxavail / 2 - 150, cursory)

			imgui.image("textures/niko.png", { 150, 150 })
			imgui.sameline()
			imgui.image("textures/yes.png", { 150, 150 })

			imgui.setcursorposition(cursorx, cursory + 160)

			imgui.text("Phoenix Engine Editor")

			imgui.urllink(GITHUB_URL)
		else
			AboutWindowOpen = false
		end

		imgui.endw()
	end
end

--local IsHoveringViewportTitlebar = false

local CommandBarCommand = ""

local function renderViewport(DeltaTime: number)
	PluginManager.OnUpdate(DeltaTime, CurrentStage)

	if BottomBarEnabled then
		local wx, wy = engine.windowsize()

		imgui.setviewportdockspace(0, 19, wx, wy - 55)
		imgui.setnextwindowposition(0, wy - 35)
		imgui.setnextwindowsize(wx, 35)
		imgui.begin("Bottom Bar", "nt | nd | nr")

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

		imgui.text(projectSavePath or "<No save path> | ")
		imgui.sameline()
		imgui.text(tostring(engine.framerate()) .. " FPS")

		imgui.endw()
	end

	--[[
	imgui.begin("Viewport", if IsHoveringViewportTitlebar then "" else "ni")

	local mx, my = input.getmousepos()
	local wx, wy = imgui.getwindowposition()
	local wsx, wsy = imgui.getwindowsize()

	if mx >= wx and mx <= wx + wsx then
		if my >= wy and my < wy + 16 then
			IsHoveringViewportTitlebar = true
		else
			IsHoveringViewportTitlebar = false
		end
	else
		IsHoveringViewportTitlebar = false
	end

	imgui.setcursorposition(0, 0)

	imgui.image("!Framebuffer:Main", { wsx, wsy }, true)

	imgui.endw()
	]]
end

function Stages.SelectProject_Enter()
	local rpstr = fs.read("recent-projects.json")

	if not rpstr then
		print("Couldn't read `recent-projects.json`")
		return
	end

	recentProjects = json.parse(rpstr)

	PluginManager.Shutdown()
end

function Stages.SelectProject_Leave()
	PluginManager.Load()
end

local RecentsHoveringOver: number? = nil

function Stages.SelectProject(DeltaTime: number)
	if imgui.beginfullscreen("Select Project", 0, 18) then
		local startPosX, startPosY = imgui.cursorposition()

		if imgui.beginchild("SidePanel", 165, 0, "b") then
			local cx, cy = imgui.cursorposition()
			imgui.setcursorposition(cx - 4, cy)

			if imgui.imagebutton("+", "textures/editor-icons/new.png", { 150, 150 }) then
				createNewProject()
			end

			cx, cy = imgui.cursorposition()
			imgui.setcursorposition(cx - 4, cy)

			if imgui.imagebutton("O", "textures/editor-icons/open.png", { 150, 150 }) then
				imgui.endchild()
				imgui.endw()

				openProject()
				return
			end
		end
		imgui.endchild()

		imgui.setcursorposition(startPosX + 170, startPosY)

		imgui.beginchild("MainSection", 0, 0, "b")

		imgui.text("Recent")
		local cx, cy = imgui.cursorposition()
		imgui.setcursorposition(cx, cy + 10)

		local windowSizeX, _ = engine.windowsize()
		local lastnl = #recentProjects

		local hoveringOverWip = nil

		cx, cy = imgui.cursorposition()
		imgui.setcursorposition(cx + 10, cy)
		local firstcx = cx + 10

		for i = #recentProjects, 1, -1 do
			local proj = recentProjects[i]
			local clicked = false
			local hovered = false
			cx, cy = imgui.cursorposition()

			imgui.dummy(300, 300)
			imgui.setcursorposition(cx, cy)

			if RecentsHoveringOver == i then
				imgui.pushstylecolor(5, { 1, 1, 1, 1 })

				imgui.setcursorposition(cx - 10, cy - 10)
				imgui.beginchild(proj.Path .. "_Selection", 270, 280, "b", "ni")
				imgui.endchild()
				imgui.setcursorposition(cx, cy)

				imgui.popstylecolor()
			end

			imgui.image(proj.Icon or FALLBACK_IMAGE, { 250, 250 })
			clicked, hovered = imgui.itemclicked(), imgui.itemhovered()
			imgui.setcursorposition(cx, cy + 250)
			imgui.text(proj.Path:sub(#("resources/scenes/") + 1, #proj.Path))
			clicked, hovered = clicked or imgui.itemclicked(), hovered or imgui.itemhovered()

			if clicked then
				imgui.endchild()
				imgui.endw();

				openProject(proj.Path)

				return
			end

			if hovered then
				hoveringOverWip = i
			end

			if (lastnl - i) * 250 + 170 + 250 + startPosX < windowSizeX - (170 * 2) - startPosX then
				imgui.setcursorposition(cx + 275 - 15, cy)
				imgui.dummy(0, 0)
				imgui.sameline()
			else
				lastnl = i-1
				_, cy = imgui.cursorposition()
				imgui.setcursorposition(firstcx, cy + 10)
			end
		end

		RecentsHoveringOver = hoveringOverWip

		imgui.dummy()
		imgui.endchild()
	end
	imgui.endw()
	
	renderMainMenuBar(DeltaTime)
end

function Stages.ProjectEditing_Enter(DeltaTime)
	assert(projDm)
	engine.setexplorerroot(projDm)

	if projWp and projWp.SceneCamera and projWp.SceneCamera:HasComponent("Camera") then
		projWp.SceneCamera.Transform = EditorCam.Transform
		EditorCam.FieldOfView = projWp.SceneCamera.FieldOfView
	end
end

function Stages.ProjectEditing(DeltaTime)
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
	assert(projDm)

	local buttonSelection = engine.showmessagebox("Save Project", "Save project before closing?", "yesno", "warning");

	if buttonSelection == 1 then
		local savePath = projectSavePath or fs.promptsave("scenes/", "*.world", "Projects")

		if savePath then
			saveToRecents(savePath)
			assert(projDm)
			scene.save({ projDm }, savePath)

			projectSavePath = nil
		else
			print("Project discarded")
		end
	else
		print("Not saving project")
	end

	projDm:Destroy()
	projWp = nil
	projCam = nil
	projDm = nil
end

local restoreState = nil

function Stages.ProjectPlaying_Enter()
	assert(projDm)
	restoreState = projDm
	
	projDm = (projDm:Duplicate() :: any) :: GameObject & DataModel
	projWp = projDm.Workspace
	projCam = projWp.SceneCamera
	EditorTl.Target = projWp
	EditorTl.Scripting = true
	
	engine.physicstimescale(1)
	engine.setexplorerroot(projDm)
	
	engine.binddatamodel(projDm) -- Set the `game` and `workspace` variables to the correct values in game scripts
	engine.pushlvm("GameScripts")
	
	if not projWp.SceneCamera then
		projWp.SceneCamera = GameObject.new("Camera")
		projWp.SceneCamera.Parent = projWp
	end

	workspace.SceneCamera = projWp.SceneCamera
	
	return projDm -- Connect to the Project's datamodel `.OnFrameBegin`
end

function Stages.ProjectPlaying(DeltaTime)
	renderMainMenuBar(DeltaTime)
	renderViewport(DeltaTime)
end

function Stages.ProjectPlaying_Leave()
	assert(restoreState)
	assert(projDm)

	projDm:Destroy()
	projDm = restoreState
	projWp = restoreState.Workspace
	projCam = projWp.SceneCamera
	EditorTl.Target = projWp
	EditorTl.Scripting = false
	engine.setexplorerroot(projDm)
	
	engine.poplvm()
	engine.binddatamodel(game)
	
	workspace.SceneCamera = EditorCam
end

changeStageTo("SelectProject")
