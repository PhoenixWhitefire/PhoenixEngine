local Mesh = workspace.Mesh
local Sound = workspace.Sound

workspace.SceneCamera.Transform = Matrix.new()
Sound.Playing = true

game.OnFrameBegin:Connect(function()
    local transform = Matrix.fromTranslation(math.sin(game.Time) * 5, 0, math.cos(game.Time) * 5)
    Mesh.Transform = transform
    Sound.Transform = transform
end)
