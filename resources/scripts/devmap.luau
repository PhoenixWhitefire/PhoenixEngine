-- devmap.luau, 10/07/2024

local Cubes = {}
local NumCubes = 0

local TotalTime = 0

local Camera = workspace.SceneCamera
Camera.GenericMovement = false

local function updateCubes(DeltaTime)
    if xisKeyPressed("t") then
        NumCubes += 1
        local newCube = GameObject.new("Primitive")
        newCube.Name = `Cube{#Cubes}`
        newCube.Parent = game.Workspace
        newCube.Size = Vector3.new(5, 5, 5)
        
        Cubes[TotalTime] = newCube
    end
    
    for spawnTime, cube in Cubes do
        local mat = cube.Transform
        xmat4_setv(mat, 3, 0, math.sin(TotalTime - spawnTime) * 15)
        xmat4_setv(mat, 3, 1, 5)
        xmat4_setv(mat, 3, 2, math.cos(TotalTime - spawnTime) * 15)
        cube.Transform = mat
    end
end

local function handleCameraMovement(DeltaTime)
    local speed = 15 * DeltaTime
    
    local moveX = (if xisKeyPressed("a") then 1 else 0) - (if xisKeyPressed("d") then 1 else 0)
    local moveY = (if xisKeyPressed("e") then 1 else 0) - (if xisKeyPressed("q") then 1 else 0)
    local moveZ = (if xisKeyPressed("w") then 1 else 0) - (if xisKeyPressed("s") then 1 else 0)
    
    local transrights = Camera.Transform
    
    xmat4_setv(transrights, 3, 0, xmat4_getv(transrights, 3, 0) + (moveX * speed))
    xmat4_setv(transrights, 3, 1, xmat4_getv(transrights, 3, 1) + (moveY * speed))
    xmat4_setv(transrights, 3, 2, xmat4_getv(transrights, 3, 2) + (moveZ * speed))
    
    Camera.Transform = transrights -- We give the Camera some rights (this means `Object_Camera` is canonically trans)
end

function Update(DeltaTime)
    TotalTime += DeltaTime
    
    updateCubes(DeltaTime)
    handleCameraMovement(DeltaTime)
end