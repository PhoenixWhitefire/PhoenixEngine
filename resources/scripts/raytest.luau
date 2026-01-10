local InputService: InputService = game:GetService("InputService") :: any

local M1WasDown = false
local M2WasDown = false
local Parts = {}

local function debugVector(v: vector, col: Color?)
    local mesh = GameObject.new("Mesh")
    mesh.Parent = workspace
    mesh.Transform = Matrix.lookAt(workspace.SceneCamera.Transform.Position, workspace.SceneCamera.Transform.Position + v) * Matrix.fromTranslation(0, 0, -vector.magnitude(v) / 2)
    mesh.Size = vector.create(1, 1, vector.magnitude(v))
    mesh.FaceCulling = 0
    mesh.Material = "boxframe"

    if col then
        mesh.Tint = col
    end

    table.insert(Parts, mesh)
end

local function debugMouseVector()
    local mouseVec = workspace:ScreenPointToVector((), 100)
    debugVector(mouseVec, nil)
end

local function debugCameraVector()
    local camVec = workspace.SceneCamera.Transform.Forward * 100
    debugVector(camVec, Color.new(1, 0, 0))
end

--workspace.SceneCamera.Transform = Matrix.fromEulerAnglesXYZ(0, math.pi, 0)

game.OnFrameBegin:Connect(function()
    if InputService.IsKeyboardSunk or InputService.IsMouseSunk then
        return
    end

    if InputService:IsMouseButtonPressed(Enum.MouseButton.Left) then
        if not M1WasDown then
            debugMouseVector()
        end

        M1WasDown = true
    else
        M1WasDown = false
    end

    if InputService:IsMouseButtonPressed(Enum.MouseButton.Right) then
        if not M2WasDown then
            debugCameraVector()
        end

        M2WasDown = true
    else
        M2WasDown = false
    end

    if InputService:IsMouseButtonPressed(Enum.Key.One) then
        for _, part in Parts do
            part:Destroy()
        end

        table.clear(Parts)
    end
end)
