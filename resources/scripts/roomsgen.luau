-- roomsgen.luau, 17/12/2024

local Rooms = workspace.Level.Rooms
local CurDoor = Rooms.Start.Door
local RoomsContainer = Rooms.Random
local PossibleRoomsArr = RoomsContainer:GetChildren()

local Player

local StartClone = Rooms.Start:Clone()
Rooms.Start.Enabled = false
StartClone.Parent = Rooms.Generated
StartClone.Enabled = true

RoomsContainer.Enabled = false

local function transformModel(Model, Trans)
	for _, v in Model:GetChildren() do
		if (v:IsA("Base3D")) then
			v.Transform *= Trans
		end
	end
end

local function genNextRoom()
	print("gen")
	local nextRoom = PossibleRoomsArr[math.random(1, #PossibleRoomsArr)]:Clone()
	print("room", nextRoom.Name)
	nextRoom.Parent = Rooms.Generated
	transformModel(nextRoom, CurDoor.Transform * (Matrix.fromEulerAnglesXYZ(0, math.rad(180), 0) * nextRoom.Entry.Transform))
	transformModel(nextRoom, Matrix.fromTranslation(0, -nextRoom.Door.Transform.Position.Y / 1.5, 0))
	CurDoor:Destroy()
	CurDoor = nextRoom.Door
	CurDoor.PhysicsCollisions = false
end

function Update()
	Player = workspace.Level.Player
	
	local inDoor = world_aabbquery(CurDoor.Transform.Position, CurDoor.Size, {})
	local generateNextRoom = false
	
	for _, v in inDoor do
		if v.ObjectId == Player.ObjectId then
			generateNextRoom = true
			break
		end
	end
	
	if generateNextRoom then
		genNextRoom()
	end
end