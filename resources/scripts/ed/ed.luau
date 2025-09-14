-- ed.luau, 14/09/2025
local CurrentStage = "None"
local StageUpdateSignal = nil
local Stages = {}

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
	
	if Stages[NewStage .. "_Enter"] then
		Stages[NewStage .. "_Enter"](CurrentStage)
	end
	
	CurrentStage = NewStage
	StageUpdateSignal = game.OnFrameBegin:Connect(Stages[CurrentStage])
end

function Stages.SelectProject(DeltaTime)
	if imgui.begin("Select Project") then
		if imgui.button("New") then
			changeStageTo("ProjectEditing")
		end
	end
	imgui.endw()
end

local projDm = nil
local projWp = nil
local projCam = nil
local projTl = nil

function Stages.ProjectEditing_Enter(PreviousStage)
	projDm = GameObject.new("DataModel")
	projWp = GameObject.new("Workspace")
	projCam = GameObject.new("Camera")
	projTl = GameObject.new("TreeLink")
	
	projWp.Parent = projDm
	projCam.Parent = projWp
	projWp.SceneCamera = projCam
	projTl.Parent = workspace
	projTl.Target = projWp
	
	engine.setexplorerroot(projDm)
end

function Stages.ProjectEditing(DeltaTime)
	if imgui.begin("Project") then
		if imgui.button("Explorer to Project DM") then
			engine.setexplorerroot(projDm)
		end
		
		if imgui.button("Explorer to Editor DM") then
			engine.setexplorerroot(game)
		end
	end
	imgui.endw()
end

changeStageTo("SelectProject")

