-- devmap.luau, 10/07/2024

local PLAYER_MOVE_SPEED = 3
local PLAYER_JUMP_IMPULSE = 150

local Cubes = {}
local NumCubes = 0

local TotalTime = 0
local PlayerFreezeTimeLeft = 5

local Camera = workspace.SceneCamera
local GetMoveDirection = require("scripts/modules/GetMoveDirection.luau")

local Player = GameObject.new("Primitive")
local LastJumped = 0

Player.Name = "Player"
Player.Size = Vector3.new(5, 15, 5)
Player.Transform = Matrix.fromTranslation(0, 25, 0)
Player.Parent = workspace

Camera.GenericMovement = false

local function rotateY(mtx, degrees)
    matrix_setv(mtx, 0, 0, math.cos(degrees))
    matrix_setv(mtx, 2, 0, math.sin(degrees))
    matrix_setv(mtx, 0, 2, -math.sin(degrees))
    matrix_setv(mtx, 2, 2, math.cos(degrees))
    
    return mtx
end

local function updateCubes(DeltaTime)
    if input_keypressed("t") then
        NumCubes += 1
        local newCube = GameObject.new("Primitive")
        newCube.Name = `Cube{NumCubes}`
        newCube.Parent = workspace.Level
        newCube.Size = Vector3.new(5, 5, 5)
        
        Cubes[TotalTime] = newCube
    end
    
    for spawnTime, cube in Cubes do
        -- 14/09/2024
        -- Indexing a `Destroy`'d GameObject will return `nil`,
        -- I'll just use `ClassName` because it's static and will never be falsy
        if not cube.ClassName then
            Cubes[spawnTime] = nil
            --continue
        end
        
        cube.Transform = Matrix.fromTranslation(math.sin(TotalTime - spawnTime) * 15, 5, math.cos(TotalTime - spawnTime) * 15)
    end
end

function Update(DeltaTime)
    if PlayerFreezeTimeLeft > 0 then
        PlayerFreezeTimeLeft -= DeltaTime
        if PlayerFreezeTimeLeft <= 0 then
            Player.PhysicsDynamics = true
        end
    end
    
    TotalTime += DeltaTime
    
    updateCubes(DeltaTime)
    
    local moveDirection = GetMoveDirection()
    local y = if input_keypressed(" ") then PLAYER_JUMP_IMPULSE else 0
    
    if TotalTime - LastJumped > 0.5 then
        LastJumped = TotalTime
    else
        y = 0
    end

    moveDirection = Vector3.new(
        if Player.LinearVelocity.X < PLAYER_MOVE_SPEED then moveDirection.X else 0,
        0,
        if Player.LinearVelocity.Z < PLAYER_MOVE_SPEED then moveDirection.Z else 0
    )
    
    Player.LinearVelocity = Vector3.new(
        Player.LinearVelocity.X + moveDirection.X * PLAYER_MOVE_SPEED,
        Player.LinearVelocity.Y + y,
        Player.LinearVelocity.Z + moveDirection.Z * PLAYER_MOVE_SPEED
    )
    
    Camera.Transform = Player.Transform * Matrix.fromTranslation(0, 5, -20) * Matrix.fromEulerAnglesXYZ(math.rad(15), 0, 0)
end