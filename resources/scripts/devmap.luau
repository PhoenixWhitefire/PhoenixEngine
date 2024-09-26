-- devmap.luau, 10/07/2024

local TotalTime = 0
local PlayerFreezeTimeLeft = 5

local Camera = workspace.SceneCamera
local CharacterControllerCreator = require("scripts/modules/character-controller.luau")

local CharacterController = CharacterControllerCreator.new(Camera)
CharacterController:InitDefault()

local DefaultCameraOffset = CharacterController.CameraOffset

local Character = CharacterController.Character

local InFreecam = false

local Light = GameObject.new("PointLight")
Light.Parent = Character

Character.Name = `Player_ID{Character.ObjectId}`
Character.Transform = Matrix.fromTranslation(8, 8, 0)

local lastSwitchedPerspectives = 0

local didNetInitClient = false
local didNetInitServer = false

local function doNetTest()
    if input_keypressed("t") and not didNetInitServer then
        didNetInitServer = true
        local numClients = 0
        
        local listenSock = net_host("27015") -- the socket clients will send connection REQUESTS to
        print("hosted. listening for connection requests on", listenSock)
        
        while true do
              local sock = net_accept(listenSock) -- accept pending connection request
              numClients += 1
              print(`client connection request accepted. client #{numClients}, communicating through socket {sock}`)
              
              net_send(sock, "Welcome to Server!")
              
              print("Client says:", table.concat({net_receive(sock, 128)}, "/"))
        
              net_send(sock, "Hello! - From Server")
              net_close(sock)
        end
        
    elseif input_keypressed("y") and not didNetInitClient then
        didNetInitClient = true
        
        local sock = net_connect("localhost", "27015")
        print("connected to", sock)
        
        net_send(sock, "Client says hi!")
        print("sent, waiting for server message...")
        
        print("Server says:", table.concat({net_receive(sock, 128)}, "/"))
        net_close(sock)
    end
end

function Update(DeltaTime)
    if PlayerFreezeTimeLeft > 0 then
        Character.PhysicsDynamics = false
        
        PlayerFreezeTimeLeft -= DeltaTime
        if PlayerFreezeTimeLeft <= 0 then
            Character.PhysicsDynamics = true
        end
    end
    
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
    
    coroutine.wrap(doNetTest)()
    
    Light.Position = Vector3.new(
        matrix_getv(Character.Transform, 3, 0),
        matrix_getv(Character.Transform, 3, 1),
        matrix_getv(Character.Transform, 3, 2)
    )
    
    if not InFreecam then
        CharacterController:Update(DeltaTime)
    end
end