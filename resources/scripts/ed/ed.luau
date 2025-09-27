-- ed.luau, 14/09/2025
type Stage = "None" | "SelectProject" | "ProjectEditing" | "ProjectPlaying"

local CurrentStage: Stage = "None"
local StageUpdateSignal: EventConnection? = nil
local Stages: { [string]: (any) -> () } = {}

local EditorCam = (workspace.SceneCamera :: any) :: GameObject & Camera
local EditorTl = (GameObject.new("TreeLink") :: any) :: GameObject & TreeLink

EditorCam.UseSimpleController = true
EditorTl.Parent = workspace
EditorTl.Scripting = false
engine.physicstimescale(0)

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
	StageUpdateSignal = targetDm.OnFrameBegin:Connect(Stages[CurrentStage])
end

local projDm: (GameObject & DataModel)? = nil
local projWp: (GameObject & Workspace)? = nil
local projCam: (GameObject & Camera)? = nil

local function createNewProject()
	projDm = (GameObject.new("DataModel") :: any) :: GameObject & DataModel
	projWp = (GameObject.new("Workspace") :: any) :: GameObject & Workspace
	projCam = (GameObject.new("Camera") :: any) :: GameObject & Camera
				
	projWp.Parent = projDm
	projCam.Parent = projWp
	projWp.SceneCamera = projCam
	EditorTl.Parent = workspace
	EditorTl.Target = projWp
	EditorTl.Scripting = false
	
	engine.setexplorerroot(projDm)
				
	changeStageTo("ProjectEditing")
end

local recentProjects: { string } = {}

local function saveRecents()
	if not fs.write("recent-projects.json", json.encode(recentProjects)) then
		print("Failed to save the Recent Projects list")
	end
end

local function saveToRecents(Path)
	local oldIndex = table.find(recentProjects, Path)
	if oldIndex then
		table.remove(recentProjects, oldIndex)
	end
	table.insert(recentProjects, Path)
	saveRecents()
end

local function openProject(ManualPath: string?)
	local path = ManualPath or fs.promptopen("resources/scenes", "world", "Worlds")[1]
	if not path then
		print("No project selected")
		return
	end
			
	projDm = (assert(scene.load(path))[1] :: any) :: GameObject & DataModel
	projWp = (assert(projDm.Workspace) :: any) :: GameObject & Workspace
	projCam = (projWp.SceneCamera :: any) :: GameObject & Camera
	EditorTl.Target = projWp
	saveToRecents(path)

	changeStageTo("ProjectEditing")
end

local aboutWindowOpen = false
local allowCopyGitHubUrl = false
local isLightMode = false

local GITHUB_URL = "https://github.com/PhoenixWhitefire/PhoenixEngine"

local function renderMainMenuBar()
	if not imgui.beginmainmenubar() then
		return
	end
	
	if imgui.beginmenu("File") then
		if imgui.menuitem("Save", CurrentStage == "ProjectEditing") then
			imgui.endmenu()
			imgui.endmainmenubar()
				
			local path = fs.promptsave("resources/scenes", "world", "Worlds")
			if not path then
				print("No path was selected for saving, cancelled")
				return
			end
				
			assert(projDm)
			scene.save({ projDm }, path)
			saveToRecents(path)
				
			return
		end

		if CurrentStage == "ProjectEditing" then
			if imgui.menuitem("Close") then
				changeStageTo("SelectProject")
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

		if not aboutWindowOpen and imgui.menuitem("About") then
			aboutWindowOpen = true
			imgui.setnextwindowopen()
		end

		local lightMode = imgui.checkbox("Light", isLightMode)
		if lightMode ~= isLightMode then
			imgui.stylecolors(if lightMode then "L" else "D")
		end
		isLightMode = lightMode
		
		imgui.endmenu()
	end
	
	imgui.endmainmenubar()

	if aboutWindowOpen then
		imgui.setnextwindowfocus()

		if imgui.begin("About") then
			local cxavail, _ = imgui.getcontentregionavail()
			local cursorx, cursory = imgui.getcursorpos()

			imgui.setcursorpos(cursorx + cxavail / 2 - 150, cursory)

			imgui.image("textures/niko.png", 150, 150)
			imgui.sameline()
			imgui.image("textures/yes.png", 150, 150)

			imgui.setcursorpos(cursorx, cursory + 160)

			imgui.text("Phoenix Engine Editor")

			if not allowCopyGitHubUrl then
				if imgui.textlink(GITHUB_URL) then
					allowCopyGitHubUrl = true
				end
			else
				imgui.inputstring("##", GITHUB_URL)
			end
		else
			aboutWindowOpen = false
			allowCopyGitHubUrl = false
		end

		imgui.endw()
	end
end

function Stages.SelectProject_Enter()
	local rpstr = fs.read("recent-projects.json")

	if not rpstr then
		print("Couldn't read `recent-projects.json`")
		return
	end

	recentProjects = json.parse(rpstr)
end

function Stages.SelectProject(DeltaTime)
	if imgui.beginfullscreen("Select Project", 0, 18) then
		local startPosX, startPosY = imgui.getcursorpos()

		if imgui.beginchild("SidePanel", 165, 0, "b") then
			if imgui.button("+", 150, 150) then
				createNewProject()
			end

			if imgui.button("O", 150, 150) then
				imgui.endchild()
				imgui.endw()

				openProject()
				return
			end
		end
		imgui.endchild()

		imgui.setcursorpos(startPosX + 170, startPosY)

		imgui.beginchild("MainSection", 0, 0, "b")

		imgui.text("Recent")

		local windowSizeX, _ = engine.getwindowsize()
		local lastnl = #recentProjects

		for i = #recentProjects, 1, -1 do
			local proj = recentProjects[i]

			if imgui.button(proj, 250, 250) then
				imgui.endchild()
				imgui.endw()

				openProject(proj)

				return
			end

			if (lastnl - i) * 250 + 170 + 250 + startPosX < windowSizeX - 170 - startPosX then
				imgui.sameline()
			else
				lastnl = i-1
			end
		end

		imgui.endchild()
	end
	imgui.endw()
	
	renderMainMenuBar()
end

local function projectWindow(InPlaytest)
	if imgui.begin("Project") then
		if imgui.button("Explorer to Project DM") then
			assert(projDm)
			engine.setexplorerroot(projDm)
		end
		
		if imgui.button("Explorer to Editor DM") then
			engine.setexplorerroot(game)
		end
		
		if InPlaytest then
			if imgui.button("Stop playtest") then
				changeStageTo("ProjectEditing")
			end
		else
			if imgui.button("Start playtest") then
				changeStageTo("ProjectPlaying")
			end
		end
	end
	imgui.endw()
	
	renderMainMenuBar()
end

function Stages.ProjectEditing_Enter(DeltaTime)
	assert(projDm)
	engine.setexplorerroot(projDm)
end

function Stages.ProjectEditing(DeltaTime)
	projectWindow(false)
	
	-- sometimes this can get called after `ProjectEditing_Leave` or something somehow
	if projWp and projWp.SceneCamera then
		projWp.SceneCamera.Transform = EditorCam.Transform
	end
end

function Stages.ProjectEditing_Leave(NextStage)
	if NextStage == "ProjectPlaying" then
		return
	end
	
	engine.setexplorerroot(game)
	assert(projDm)
	projDm:Destroy()
	projWp = nil
	projCam = nil
	projDm = nil
end

local restoreState = nil

function Stages.ProjectPlaying_Enter()
	assert(projDm)
	restoreState = projDm
	
	projDm = projDm:Duplicate()
	projWp = projDm.Workspace
	projCam = projWp.SceneCamera
	EditorTl.Target = projWp
	EditorTl.Scripting = true
	
	engine.physicstimescale(1)
	engine.setexplorerroot(projDm)
	
	engine.binddatamodel(projDm) -- Set the `game` and `workspace` variables to the correct values in game scripts
	engine.pushlvm("GameScripts")
	
	workspace.SceneCamera = projWp.SceneCamera
	
	return projDm -- Connect to the Project's datamodel `.OnFrameBegin`
end

function Stages.ProjectPlaying(DeltaTime)
	projectWindow(true)
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
