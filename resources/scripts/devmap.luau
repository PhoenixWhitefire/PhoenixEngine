-- devmap.luau, 10/07/2024

local TotalTime = 0

local Camera = workspace.SceneCamera
local CharacterControllerCreator = require("scripts/modules/character-controller.luau")
--local RoomGen = require("scripts/roomsgen.luau")

local CharacterController = CharacterControllerCreator.new(Camera)
local Character = CharacterController.Character

local DefaultCameraOffset = CharacterController.CameraOffset

local InFreecam = false

local lastSwitchedPerspectives = 0

local SwitchToFreecamNextFrame = 0

local function reset()
	CharacterController:InitDefault()
	Character = CharacterController.Character
	Character.Serializes = false
	
	Character.Parent = workspace
	Character.Name = `Player`
	Character.Transform = Matrix.fromTranslation(8, 10, 0)
	
	DefaultCameraOffset = CharacterController.CameraOffset
end

local function vecMul(v1, v2)
	return Vector3.new(v1.X * v2.X, v1.Y * v2.Y, v1.Z * v2.Z)
end

function Update(DeltaTime)
	DeltaTime = math.clamp(DeltaTime, 0, 1/30)
	TotalTime += DeltaTime
	
	if SwitchToFreecamNextFrame > 0 then
		SwitchToFreecamNextFrame -= 1
		if SwitchToFreecamNextFrame <= 0 then
			Camera.UseSimpleController = true
		end
	end
	
	if input_keypressed("t") and TotalTime - lastSwitchedPerspectives > 0.5 then
		reset()
		lastSwitchedPerspectives = TotalTime
	end
	
	if input_keypressed("f") and TotalTime - lastSwitchedPerspectives > 0.5 then
		lastSwitchedPerspectives = TotalTime
		
		InFreecam = not InFreecam
		if InFreecam then
			input_mouse_setlocked(false)
			SwitchToFreecamNextFrame = 2
		else
			Camera.UseSimpleController = false
		end
	end
	
	if input_keypressed("p") and TotalTime - lastSwitchedPerspectives > 0.5 then
		CharacterController.FirstPerson = not CharacterController.FirstPerson
		lastSwitchedPerspectives = TotalTime
		
		if CharacterController.FirstPerson then
			CharacterController.CameraOffset = DefaultCameraOffset
		else
			CharacterController.CameraOffset = DefaultCameraOffset * Matrix.fromTranslation(0, 2, -20)
			--CharacterController.CameraTargetOffset = DefaultCameraOffset
		end
	end
	
	if not InFreecam then
		--CharacterController.CameraOffset = DefaultCameraOffset * Matrix.fromEulerAnglesXYZ(0, 0, Character.LinearVelocity.X)
		CharacterController:Update(DeltaTime)
		
		if input_keypressed("e") and TotalTime - lastSwitchedPerspectives > 0.25 then
			lastSwitchedPerspectives = TotalTime
			local bruh = Character:Duplicate()
			bruh.Name = "bruh"
			bruh.Transform = Matrix.fromTranslation((Character.Transform * Matrix.fromTranslation(0, 0, 7)).Position)
			bruh.LinearVelocity = Character.Transform.Forward * 100
			bruh.PhysicsDynamics = true
			bruh.Parent = workspace
		end
	end
	
	workspace.Level.woodplanks.Transform *= Matrix.fromEulerAnglesXYZ(0, math.rad(DeltaTime * 15), 0)
	
	--RoomGen()
	--NodeEditor()
end

reset()