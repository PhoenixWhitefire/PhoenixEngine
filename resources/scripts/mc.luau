-- mc.luau, 18/09/2024

local Cubes = {}
local NumCubes = 0

local TotalTime = 0

local Camera = workspace.SceneCamera

function Update(DeltaTime)
    TotalTime += DeltaTime
end

local seed = math.random(1, 1000)

for x = -10, 10 do
    for z = -10, 10 do
        local cube = GameObject.new("Primitive")
        local mtx = cube.Transform
        
        matrix_setv(mtx, 3, 0, x)
        matrix_setv(mtx, 3, 2, z)
        matrix_setv(mtx, 3, 1, math.noise(x + .1, z, seed) * 5)
        cube.Transform = mtx
        
        cube.Parent = workspace.Level
    end
end