-- ed.luau, 14/09/2025
local CurrentStage = "None"
local StageUpdateSignal = nil
local Stages = {}

local EditorCam = workspace.SceneCamera
local EditorTl = GameObject.new("TreeLink")

EditorCam.UseSimpleController = true
EditorTl.Parent = workspace
EditorTl.Scripting = false
engine.physicstimescale(0)

local function changeStageTo(NewStage)
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

local projDm = nil
local projWp = nil
local projCam = nil

function Stages.SelectProject(DeltaTime)
	if imgui.begin("Select Project") then
		if imgui.button("New") then
			changeStageTo("CreateNewProject")
		end
		
		if imgui.button("Open") then
			imgui.endw()
			
			local path = fs.promptopen("resources/scenes", "world", "Worlds")[1]
			if not path then
				print("No project selected")
				return
			end
			
			projDm = scene.load(path)[1]
			projWp = projDm.Workspace
			projCam = projWp.SceneCamera
			EditorTl.Target = projWp
			
			changeStageTo("ProjectEditing")
			
			return
		end
	end
	imgui.endw()
end

function Stages.CreateNewProject_Enter()
	projDm = GameObject.new("DataModel")
	projWp = GameObject.new("Workspace")
	projCam = GameObject.new("Camera")
	
	projWp.Parent = projDm
	projCam.Parent = projWp
	projWp.SceneCamera = projCam
	EditorTl.Parent = workspace
	EditorTl.Target = projWp
	EditorTl.Scripting = false
	
	engine.setexplorerroot(projDm)
end

function Stages.CreateNewProject()
	changeStageTo("ProjectEditing")
end

local function projectWindow(InPlaytest)
	if imgui.begin("Project") then
		if imgui.button("Explorer to Project DM") then
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
			
			if imgui.button("Save") then
				imgui.endw()
				
				local path = fs.promptsave("resources/scenes", "world", "Worlds")
				if not path then
					print("No path was selected for saving, cancelled")
					return
				end
				
				scene.save({ projDm }, path)
				
				return
			end
			
			if imgui.button("Close") then
				changeStageTo("SelectProject")
			end
		end
	end
	imgui.endw()
end

function Stages.ProjectEditing_Enter(DeltaTime)
	engine.setexplorerroot(projDm)
end

function Stages.ProjectEditing(DeltaTime)
	projectWindow(false)
	
	-- sometimes this can get called after `ProjectEditing_Leave` or something somehow
	if projWp then
		projWp.SceneCamera.Transform = EditorCam.Transform
	end
end

function Stages.ProjectEditing_Leave(NextStage)
	if NextStage == "ProjectPlaying" then
		return
	end
	
	engine.setexplorerroot(game)
	projDm:Destroy()
	projWp = nil
	projCam = nil
	projDm = nil
end

local restoreState = nil

function Stages.ProjectPlaying_Enter()
	restoreState = projDm
	
	projDm = projDm:Duplicate()
	projWp = projDm.Workspace
	projCam = projWp.SceneCamera
	EditorTl.Target = projWp
	EditorTl.Scripting = true
	
	engine.physicstimescale(1)
	engine.setexplorerroot(projDm)
	
	engine.binddatamodel(projDm) -- Set the `game` and `workspace` variables to the correct values in game scripts
	engine.pushlvm()
	
	workspace.SceneCamera = projWp.SceneCamera
	
	return projDm -- Connect to the Project's datamodel `.OnFrameBegin`
end

function Stages.ProjectPlaying(DeltaTime)
	projectWindow(true)
end

function Stages.ProjectPlaying_Leave()
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

