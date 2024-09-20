-- devmap.luau, 10/07/2024

local PLAYER_MOVE_SPEED = 2
local PLAYER_JUMP_IMPULSE = 150

local TotalTime = 0
local PlayerFreezeTimeLeft = 3

local Camera = workspace.SceneCamera
local GetMoveDirection = require("scripts/modules/GetMoveDirection.luau")

local Player = GameObject.new("Primitive")
local LastCubeFired = 0
local LastJumped = 0

Player.Name = "Player"
Player.Size = Vector3.new(5, 15, 5)
Player.Transform = Matrix.fromTranslation(0, 25, 0)
Player.Parent = workspace

Camera.GenericMovement = false

local function fireCube()
    if not input_keypressed("e") or TotalTime - LastCubeFired < 0.5 then
        return
    end
    LastCubeFired = TotalTime
    
    local lx = matrix_getv(Camera.Transform, 2, 0)
    local ly = matrix_getv(Camera.Transform, 2, 1)
    local lz = matrix_getv(Camera.Transform, 2, 2)
    
    local newCube = GameObject.new("Primitive")
    newCube.Transform = Camera.Transform * Matrix.fromTranslation(lx, ly, lz)
    newCube.PhysicsCollisions = false
    newCube.PhysicsDynamics = true
    newCube.LinearVelocity = Vector3.new(lx * 50, ly * 50, lz * 50)
    newCube.Parent = workspace
end

function Update(DeltaTime)
    if PlayerFreezeTimeLeft > 0 then
        PlayerFreezeTimeLeft -= DeltaTime
        if PlayerFreezeTimeLeft <= 0 then
            Player.PhysicsDynamics = true
        end
    end
    
    TotalTime += DeltaTime
    
    fireCube()
    
    local moveDirection = GetMoveDirection()
    local y = if input_keypressed(" ") then PLAYER_JUMP_IMPULSE else 0
    
    if TotalTime - LastJumped > 0.5 then
        LastJumped = TotalTime
    else
        y = 0
    end

    local currentVelocity = Player.LinearVelocity
    
    Player.LinearVelocity = Vector3.new(
        Player.LinearVelocity.X + moveDirection.X * PLAYER_MOVE_SPEED,
        Player.LinearVelocity.Y + y,
        Player.LinearVelocity.Z + moveDirection.Z * PLAYER_MOVE_SPEED
    )
    
    Camera.Transform = Player.Transform * Matrix.fromTranslation(0, 5, -20) * Matrix.fromEulerAnglesXYZ(math.rad(15), 0, 0)
end