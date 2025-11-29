local M1WasDown = false
local M2WasDown = false
local Parts = {}

local function debugVector(v: vector, col: Color?)
    local mesh = GameObject.new("Mesh" :: "Mesh")
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
    local mouseVec = workspace:ScreenPointToVector(input.mouseposition(), 100)
    debugVector(mouseVec, nil)
end

local function debugCameraVector()
    local camVec = workspace.SceneCamera.Transform.Forward * 100
    debugVector(camVec, Color.new(1, 0, 0))
end

--workspace.SceneCamera.Transform = Matrix.fromEulerAnglesXYZ(0, math.pi, 0)

game.OnFrameBegin:Connect(function()
    if input.mousedown('l') and not input.guihandledm() then
        if not M1WasDown then
            debugMouseVector()
        end

        M1WasDown = true
    else
        M1WasDown = false
    end

    if input.mousedown('r') and not input.guihandledm() then
        if not M2WasDown then
            debugCameraVector()
        end

        M2WasDown = true
    else
        M2WasDown = false
    end

    if input.keypressed(Enum.Key.One) then
        for _, part in Parts do
            part:Destroy()
        end

        table.clear(Parts)
    end
end)
