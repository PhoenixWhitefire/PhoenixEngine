-- devmap.luau, 10/07/2024

local TotalTime = 0

local Camera = workspace.SceneCamera
local CharacterControllerCreator = require("scripts/modules/character-controller.luau")

local CharacterController = CharacterControllerCreator.new(Camera)
CharacterController:InitDefault()

local DefaultCameraOffset = CharacterController.CameraOffset

local Character = CharacterController.Character

local InFreecam = false

local lastSwitchedPerspectives = 0

local ModelImporterPath = "models/EmbeddedTexture.glb"
local ModelImporterScale = 0.1

local SwitchToFreecamNextFrame = 0

local Light = GameObject.new("PointLight")
Light.Brightness = 5
Light.Range = 1.5
Light.Parent = Character

Character.Name = `Player_ID{Character.ObjectId}`
Character.Transform = Matrix.fromTranslation(8, 8, 0)

function Update(DeltaTime)
    DeltaTime = math.clamp(DeltaTime, 0, 1/30)
    TotalTime += DeltaTime
    
    if SwitchToFreecamNextFrame > 0 then
        SwitchToFreecamNextFrame -= 1
        if SwitchToFreecamNextFrame <= 0 then
            Camera.UseSimpleController = true
        end
    end
    
    if input_keypressed("f") and TotalTime - lastSwitchedPerspectives > 0.5 then
        lastSwitchedPerspectives = TotalTime
        
        InFreecam = not InFreecam
        if InFreecam then
            input_mouse_setlocked(false)
            SwitchToFreecamNextFrame = 2
        else
            Camera.UseSimpleController = false
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
        
        if input_keypressed("e") and TotalTime - lastSwitchedPerspectives > 0.25 then
            lastSwitchedPerspectives = TotalTime
            local bruh = GameObject.new("Primitive")
            bruh.Transform = Matrix.fromTranslation((Character.Transform * Matrix.fromTranslation(0, 0, 7)).Position)
            bruh.LinearVelocity = Character.Transform.Forward * 100
            bruh.Size = Character.Size
            bruh.PhysicsDynamics = true
            bruh.Parent = workspace
        end
    end
    
    imgui_begin("Model Importer")
    
    ModelImporterPath = imgui_input_text("Path", ModelImporterPath)
    ModelImporterScale = imgui_input_float("Scale", ModelImporterScale)
    
    if imgui_button("Import") then
        local pathSplit = string.split(ModelImporterPath, "/")
        
        local parts = model_import(ModelImporterPath)
        local model = GameObject.new("Model")
        model.Name = pathSplit[#pathSplit]
        model.Parent = workspace
        
        for _, part in parts do
            part.Size *= ModelImporterScale
            part.Parent = model
        end
    end
    
    imgui_end()
end
