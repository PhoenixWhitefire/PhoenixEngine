-- ed.luau, 14/09/2025
type Stage = "None" | "SelectProject" | "ProjectEditing" | "ProjectPlaying"

local FALLBACK_IMAGE = "textures/fallback-x512.png"

local CurrentStage: Stage = "None"
local StageUpdateSignal: EventConnection? = nil
local Stages: { [string]: (any) -> () } = {}

local EditorCam = workspace.SceneCamera
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
	
	CurrentStage = NewStage
	StageUpdateSignal = targetDm.OnFrameBegin:Connect(function()
		MouseClick = (not WasHoldingDownM1) and input.mousedown("l")

		Stages[CurrentStage]()

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
	local path = ManualPath or fs.promptopen("scenes/", "world", "Worlds")[1]
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

	local file, error = scene.load(path)

	if not file then
		error = error or "Unknown error"
		engine.showmessagebox("Project Error", "Couldn't open project: " .. error, "e")

		return
	end

	projDm = (assert(file[1]) :: any) :: GameObject & DataModel
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

local function tabButton(Text: string, Icon: string, TintColor: { number }?)
	if Text == TabButtonHovering then
		imgui.pushstylecolor(5, { 1, 1, 1, 1 })
	end

	imgui.beginchild(Text .. "_Selection", 120, 150, "b")

	local clicked, hovered = false, false
	imgui.image(Icon, { 100, 100 }, false, TintColor)
	clicked, hovered = imgui.itemclicked(), imgui.itemhovered()
	imgui.text(Text)
	clicked, hovered = clicked or imgui.itemclicked(), hovered or imgui.itemhovered()

	local windowHovered = imgui.windowhovered()

	imgui.endchild()

	if hovered or windowHovered then
		WipTabButtonHovering = Text
	end

	if Text == TabButtonHovering then
		imgui.popstylecolor()

		if MouseClick then
			return true
		end
	end

	return clicked
end

local function homeTab()
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
end

local function toolsTab()
	for _, tool in ToolOrder do
		local name = tool:sub(#("Tool_") + 1, #tool)
		local enabled = engine.toolenabled(tool)
		local tint = if enabled then { 1, 1, 1, 1 } else { 0.5, 0.5, 0.5, 1 }

		if tabButton(`{name}\n{ if enabled then "Enabled" else "Disabled" }`, `textures/editor-icons/{name}.png`, tint) then
			engine.settoolenabled(tool, not enabled)
			conf.save()
		end

		imgui.sameline()
	end

	local tint = if BottomBarEnabled then { 1, 1, 1, 1 } else { 0.5, 0.5, 0.5, 1 }

	if tabButton(`Bottom Bar\n{ if BottomBarEnabled then "Enabled" else "Disabled" }`, `textures/editor-icons/fallback.png`, tint) then
		BottomBarEnabled = not BottomBarEnabled
		conf.set("Ed_Tool_BottomBarEnabled", BottomBarEnabled)
		conf.save()
	end
end

local RibbonTabs = {
	Home = homeTab,
	Tools = toolsTab
}

local AboutWindowOpen = false
local AllowCopyGitHubUrl = false
local IsLightMode = false
local CurrentRibbonTab = "Home"

local GITHUB_URL = "https://github.com/PhoenixWhitefire/PhoenixEngine"

local HoveringOverMenuBar = false
local HoveringOverRibbonTab = false
local RibbonPopupDistance = 24

local function renderMainMenuBar()
	if not imgui.beginmainmenubar() then
		return
	end
	local _, cursorY = input.mouseposition()
	HoveringOverMenuBar = cursorY < RibbonPopupDistance
	
	if imgui.beginmenu("File") then
		if imgui.menuitem("Save", CurrentStage == "ProjectEditing") then
			imgui.endmenu()
			imgui.endmainmenubar()
				
			local path = fs.promptsave("scenes/", "world", "Worlds")
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

				projectIcon = fs.promptopen("textures/", "png;jpg;jpeg", "Images")[1]

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
			imgui.stylecolors(if lightMode then "L" else "D")
		end
		IsLightMode = lightMode
		
		imgui.endmenu()
	end

	if CurrentStage ~= "SelectProject" then
		imgui.separator()

		WipTabButtonHovering = nil
		WindowHovering = false

		for tabName, _ in RibbonTabs do
			if imgui.menuitem(if CurrentRibbonTab == tabName then `[{tabName}]` else tabName) then
				CurrentRibbonTab = tabName
			end
		end

		if HoveringOverMenuBar or HoveringOverRibbonTab or TabButtonHovering ~= nil or WindowHovering then
			local windowWidth, _ = engine.windowsize()

			local TAB_HEIGHT = 175

			imgui.setnextwindowposition(0, 16)
			imgui.setnextwindowsize(windowWidth, TAB_HEIGHT)

			imgui.begin("RibbonTabContents", "nt | nr | nm | nc | ns | h")
			HoveringOverRibbonTab = imgui.windowhovered() or imgui.anyitemactive()
			RibbonTabs[CurrentRibbonTab]()
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

			if not AllowCopyGitHubUrl then
				if imgui.textlink(GITHUB_URL) then
					AllowCopyGitHubUrl = true
				end
			else
				imgui.inputstring("##", GITHUB_URL)
			end
		else
			AboutWindowOpen = false
			AllowCopyGitHubUrl = false
		end

		imgui.endw()
	end
end

--local IsHoveringViewportTitlebar = false

local CommandBarCommand = ""

local function renderViewport()
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

		imgui.text(if projectSavePath then projectSavePath else "<No save path> | ")
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
end

local RecentsHoveringOver: number? = nil

function Stages.SelectProject(DeltaTime)
	if imgui.beginfullscreen("Select Project", 0, 18) then
		local startPosX, startPosY = imgui.cursorposition()

		if imgui.beginchild("SidePanel", 165, 0, "b") then
			if imgui.imagebutton("+", "textures/editor-icons/new.png", { 150, 150 }) then
				createNewProject()
			end

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

			if (lastnl - i) * 250 + 170 + 250 + startPosX < windowSizeX - 170 - startPosX then
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
	
	renderMainMenuBar()
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

	renderMainMenuBar()
	renderViewport()
end

function Stages.ProjectEditing_Leave(NextStage)
	if NextStage == "ProjectPlaying" then
		return
	end
	
	engine.setexplorerroot(game)
	assert(projDm)

	local savePath = projectSavePath or fs.promptsave("scenes/", "world", "Projects")

	if savePath then
		saveToRecents(savePath)
		assert(projDm)
		scene.save({ projDm }, savePath)

		projectSavePath = nil
	else
		print("Project discarded")
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
	renderMainMenuBar()
	renderViewport()
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
