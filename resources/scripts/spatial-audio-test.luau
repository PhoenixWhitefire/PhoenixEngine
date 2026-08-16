local Workspace = game:GetService("Workspace")

local Mesh = Workspace.Mesh
local Sound = Workspace.Sound

Workspace.SceneCamera.Transform = Matrix.new()
Sound.Playing = true

game.OnFrameBegin:Connect(function()
    local transform = Matrix.translated(math.sin(game.Time) * 5, 0, math.cos(game.Time) * 5)
    Mesh.Transform = transform
    Sound.Transform = transform
end)
