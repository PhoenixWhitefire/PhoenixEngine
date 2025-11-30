-- minesweeper.luau, 06/07/2025

local CellColors = {
    Color.new(0, 0, 1),
    Color.new(0, 1, 0),
    Color.new(1, 0, 0),
    Color.new(0, 0, 0.5),
    Color.new(0.5, 0, 0),
    Color.new(0, 0.5, 0.5),
    Color.new(0.3, 0.3, 0.3),
    Color.new(0.5, 0.5, 0.5)
}

local GridModel = GameObject.new("Model" :: "Model")
GridModel.Name = "Grid"
GridModel.Parent = workspace

local CellTemplate = GameObject.new("Mesh" :: "Mesh")
CellTemplate.Parent = GridModel

workspace.SceneCamera.UseSimpleController = false
input.setmousegrabbed(false)

local Grid = {}
local GRID_SIZE = 5

for x = 1, GRID_SIZE do
    Grid[x] = {}

    for y = 1, GRID_SIZE do
        local cell = CellTemplate:Duplicate()
        cell.Name = `{x}_{y}`
        cell.Transform = Matrix.fromTranslation(vector.create(x, y))

        Grid[x][y] = { cell, if math.random(1, 5) == 1 then true else false }
    end
end

local function setColorOfCell(x, y)
    local cellData = Grid[x][y]

    if cellData[2] == true then
        cellData[1].Tint = Color.new(0, 0, 0)

    else
        local total = 0
    
        for ox = -1, 1 do
            if not Grid[x + ox] then
                continue
            end
        
            for oy = -1, 1 do
                local neighbourData = Grid[x + ox][y + oy]
            
                if neighbourData and neighbourData[2] == true then
                    total += 1
                end
            end
        end
    
        if total > 0 then
            cellData[1].Tint = CellColors[total]
        else
            cellData[1].Tint = Color.new(1, 1, 1)
        end
    end
end

CellTemplate:Destroy()

workspace.SceneCamera.Transform = Matrix.fromTranslation(GRID_SIZE / 2, GRID_SIZE / 2, 5) * Matrix.fromEulerAnglesXYZ(0, math.rad(180), 0)

local WasLmbPressed = false

local function handleClick()
    local mouseX, mouseY = input.getmousepos()
    local mouseRayVec = workspace:ScreenPointToVector(mouseX, mouseY, 100)
    
    local hit = world.raycast(workspace.SceneCamera.Transform.Position, mouseRayVec, {})

    if hit then
        local prevCol = hit.Object.Tint
        hit.Object.Tint = Color.new(1, 0, 0)
        sleep(0.5)
        hit.Object.Tint = prevCol
    end
end

function Update(DeltaTime)
    if input.mousedown(Enum.MouseButton.Left) then
        if not WasLmbPressed then
            coroutine.wrap(handleClick)()
        end

        WasLmbPressed = true
    else
        WasLmbPressed = false
    end
end
