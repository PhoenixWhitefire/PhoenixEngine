-- devmap.luau, 10/07/2024

local Cubes = {}
local NumCubes = 0

local TotalTime = 0

local Camera = workspace.SceneCamera
Camera.GenericMovement = false

local function updateCubes(DeltaTime)
    if input_keypressed("t") then
        NumCubes += 1
        local newCube = GameObject.new("Primitive")
        newCube.Name = `Cube{#Cubes}`
        newCube.Parent = game.Workspace
        newCube.Size = Vector3.new(5, 5, 5)
        
        Cubes[TotalTime] = newCube
    end
    
    for spawnTime, cube in Cubes do
        local mat = cube.Transform
        matrix_setv(mat, 3, 0, math.sin(TotalTime - spawnTime) * 15)
        matrix_setv(mat, 3, 1, 5)
        matrix_setv(mat, 3, 2, math.cos(TotalTime - spawnTime) * 15)
        cube.Transform = mat
    end
end

local function handleCameraMovement(DeltaTime)
    local speed = 15 * DeltaTime
    
    local moveX = (if input_keypressed("a") then 1 else 0) - (if input_keypressed("d") then 1 else 0)
    local moveY = (if input_keypressed("e") then 1 else 0) - (if input_keypressed("q") then 1 else 0)
    local moveZ = (if input_keypressed("w") then 1 else 0) - (if input_keypressed("s") then 1 else 0)
    
    local transrights = Camera.Transform
    
    matrix_setv(transrights, 3, 0, matrix_getv(transrights, 3, 0) + (moveX * speed))
    matrix_setv(transrights, 3, 1, matrix_getv(transrights, 3, 1) + (moveY * speed))
    matrix_setv(transrights, 3, 2, matrix_getv(transrights, 3, 2) + (moveZ * speed))
    
    Camera.Transform = transrights -- We give the Camera some rights (this means `Object_Camera` is canonically trans)
end

function Update(DeltaTime)
    TotalTime += DeltaTime
    
    updateCubes(DeltaTime)
    handleCameraMovement(DeltaTime)
end