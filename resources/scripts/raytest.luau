local PlayerInput = game:GetService("PlayerInput")
local Workspace = game:GetService("Workspace")

local M1WasDown = false
local M2WasDown = false
local Parts = {}

local function debugVector(v: vector, col: Color?)
    local mesh = GameObject.fromTemplate("Mesh")
    mesh.Parent = Workspace
    mesh.Transform = Matrix.lookAt(Workspace.SceneCamera.Transform.Translation, Workspace.SceneCamera.Transform.Translation + v) * Matrix.translated(0, 0, -vector.magnitude(v) / 2)
    mesh.Transform = mesh.Transform:WithScale(vector.create(1, 1, vector.magnitude(v)))
    mesh.FaceCulling = Enum.FaceCulling.None
    mesh.Material = "boxframe"

    if col then
        mesh.Tint = col
    end

    table.insert(Parts, mesh)
end

local function debugMouseVector()
    local mouseVec = Workspace:ScreenPointToVector((), 100)
    debugVector(mouseVec, nil)
end

local function debugCameraVector()
    local camVec = Workspace.SceneCamera.Transform.Forward * 100
    debugVector(camVec, Color.new(1, 0, 0))
end

--workspace.SceneCamera.Transform = Matrix.fromEulerAnglesXYZ(0, math.pi, 0)

game.OnFrameBegin:Connect(function()
    if PlayerInput.IsKeyboardSunk or PlayerInput.IsMouseSunk then
        return
    end

    if PlayerInput:IsMouseButtonPressed(Enum.MouseButton.Left) then
        if not M1WasDown then
            debugMouseVector()
        end

        M1WasDown = true
    else
        M1WasDown = false
    end

    if PlayerInput:IsMouseButtonPressed(Enum.MouseButton.Right) then
        if not M2WasDown then
            debugCameraVector()
        end

        M2WasDown = true
    else
        M2WasDown = false
    end

    if PlayerInput:IsKeyPressed(Enum.Key.One) then
        for _, part in Parts do
            part:Destroy()
        end

        table.clear(Parts)
    end
end)
