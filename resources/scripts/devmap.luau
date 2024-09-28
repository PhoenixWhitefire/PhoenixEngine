-- devmap.luau, 10/07/2024

local TotalTime = 0

local Camera = workspace.SceneCamera
local CharacterControllerCreator = require("scripts/modules/character-controller.luau")

local CharacterController = CharacterControllerCreator.new(Camera)
CharacterController:InitDefault()

local DefaultCameraOffset = CharacterController.CameraOffset

local Character = CharacterController.Character

local UIText = GameObject.new("Mesh")

local InFreecam = false

local lastSwitchedPerspectives = 0

local Light = GameObject.new("PointLight")
Light.Brightness = 5
Light.Range = 1.5
Light.Parent = Character

Character.Name = `Player_ID{Character.ObjectId}`
Character.Transform = Matrix.fromTranslation(8, 8, 0)

UIText.Name = "UITextTest"
UIText.Material = "ui"
UIText.PhysicsCollisions = false
UIText.Parent = workspace.Camera

local RaycastTest = GameObject.new("Primitive")
RaycastTest.Size = Vector3.new(5, 5, 5)
RaycastTest.PhysicsCollisions = false
RaycastTest.ColorRGB = Color.new(1, 1, 0)
RaycastTest.Parent = workspace

local QuadMesh = {
    Vertices = {
        {
            Position = Vector3.new(-.5, .5, 0),
            Normal = Vector3.new(0, 0, -1),
            Color = Vector3.new(1, 1, 1),
            UV = {0, 1}
        },
        {
            Position = Vector3.new(-.5, -.5, 0),
            Normal = Vector3.new(0, 0, -1),
            Color = Vector3.new(1, 1, 1),
            UV = {1, 1}
        },
        {
            Position = Vector3.new(.5, .5, 0),
            Normal = Vector3.new(0, 0, -1),
            Color = Vector3.new(1, 1, 1),
            UV = {0, 0}
        },
        {
            Position = Vector3.new(.5, -.5, 0),
            Normal = Vector3.new(0, 0, -1),
            Color = Vector3.new(1, 1, 1),
            UV = {1, 0}
        },
    },
    Indices = { 0, 1, 3, 3, 2, 0 }
}

--[[

local Lights = {}

local lcols = {Color.new(1, 0, 0), Color.new(0, 1, 0), Color.new(0, 0, 1), Color.new(1, 0, 1)}
local lbrs = {12, 16, 36, 8}

for i=1, 4 do
    local pl = GameObject.new("PointLight")
    pl.Parent = workspace
    pl.LightColor = lcols[i]
    pl.Brightness = lbrs[i]
    pl.Range = -1
    local c = GameObject.new("Primitive")
    c.ColorRGB = pl.LightColor
    c.Parent = workspace
    table.insert(Lights, {c, pl})
end

mesh_set("$quad", QuadMesh)
]]--

--UIText.Asset = "$quad"

function Update(DeltaTime)
    TotalTime += DeltaTime
    
    if input_keypressed("f") and TotalTime - lastSwitchedPerspectives > 0.5 then
        InFreecam = not InFreecam
        if InFreecam then
            Camera.GenericMovement = true
        else
            Camera.GenericMovement = false
        end
    end
    
    if input_keypressed("p") and TotalTime - lastSwitchedPerspectives > 0.5 then
        CharacterController.FirstPerson = not CharacterController.FirstPerson
        lastSwitchedPerspectives = TotalTime
        
        if CharacterController.FirstPerson then
            CharacterController.CameraOffset = DefaultCameraOffset
        else
            CharacterController.CameraOffset = DefaultCameraOffset * Matrix.fromTranslation(0, -5, -15)
            CharacterController.CameraTargetOffset = DefaultCameraOffset
        end
    end
    
    if not InFreecam then
        CharacterController:Update(DeltaTime)
        
        --UIText.Transform = Camera.Transform * Matrix.fromTranslation(0, 0, 1)
        Light.Position = Camera.Transform.Position
        
        local hit = world_raycast(
            Camera.Transform.Position,
            Camera.Transform.Forward * 25,
            {Character, RaycastTest}
        )
        
        if hit then
            RaycastTest.ColorRGB = hit.Object.ColorRGB
            RaycastTest.Transform = Matrix.fromTranslation(
                hit.Position.X + hit.Normal.X,
                hit.Position.Y + hit.Normal.Y,
                hit.Position.Z + hit.Normal.Z
            )
        end
    end
    
    --[[
    for i, l in Lights do
        local plTransform = Matrix.fromTranslation(0, 0, 0)
                        * Matrix.fromEulerAnglesXYZ(#Lights / 2 - i, 0, TotalTime / 5 * (#Lights / 2 - 1))
                        * Matrix.fromTranslation(math.sin(TotalTime) * 10 * i, 0, math.sin(TotalTime) * 10 * i)
        l[2].Position = Vector3.new(matrix_getv(plTransform, 3, 0), matrix_getv(plTransform, 3, 1), matrix_getv(plTransform, 3, 2))
        l[1].Transform = plTransform
    end
    --]]
end