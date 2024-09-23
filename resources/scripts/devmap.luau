-- devmap.luau, 10/07/2024

local PLAYER_MOVE_SPEED = 10
local PLAYER_JUMP_IMPULSE = 150
local PLAYER_ROTATE_SPEED = math.rad(60)

local TotalTime = 0
local PlayerFreezeTimeLeft = 5

local Camera = workspace.SceneCamera
local GetMoveDirection = require("scripts/modules/GetMoveDirection.luau")

local Player = GameObject.new("Primitive")
local LastJumped = 0
local CameraPitch = 0
local PrevMouseX, PrevMouseY = 0, 0
local IsMouseLocked = false

Player.Name = `Player_ID{Player.ObjectId}`
Player.Size = Vector3.new(5, 15, 5)
Player.Transform = Matrix.fromTranslation(8, 8, 0)
Player.Friction = 5
Player.Parent = workspace

Camera.GenericMovement = false

local function getRotationVector(DeltaTime)
    if input_mouse_bdown(true) then
        IsMouseLocked = true -- lock mouse with left-click
        
    elseif input_mouse_bdown(false) then
        IsMouseLocked = false -- unlock mouse with right-click
    end
    input_mouse_setlocked(IsMouseLocked)
    
    local rx, ry = 0, 0
    local mouseX, mouseY = input_mouse_getpos()
    
    if IsMouseLocked then
        rx, ry = (mouseY - PrevMouseY) / 400, (PrevMouseX - mouseX) / 400
    end
    
    PrevMouseX, PrevMouseY = mouseX, mouseY
    
    return rx, ry
end

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
        PlayerFreezeTimeLeft -= DeltaTime
        if PlayerFreezeTimeLeft <= 0 then
            Player.PhysicsDynamics = true
        end
    end
    
    TotalTime += DeltaTime
    
    coroutine.wrap(doNetTest)()
    
    local moveDirection = GetMoveDirection()
    local y = if input_keypressed(" ") then PLAYER_JUMP_IMPULSE else 0
    
    if TotalTime - LastJumped > 0.5 then
        LastJumped = TotalTime
    else
        y = 0
    end

    local currentVelocity = Player.LinearVelocity
    local rx, ry = getRotationVector(DeltaTime)
    
    local velMtx = Player.Transform * Matrix.fromTranslation(
        moveDirection.X * PLAYER_MOVE_SPEED,
        y,
        moveDirection.Z * PLAYER_MOVE_SPEED
    )
    Player.LinearVelocity = Vector3.new(
        Player.LinearVelocity.X + (matrix_getv(velMtx, 3, 0) - matrix_getv(Player.Transform, 3, 0)),
        Player.LinearVelocity.Y + (matrix_getv(velMtx, 3, 1) - matrix_getv(Player.Transform, 3, 1)),
        Player.LinearVelocity.Z + (matrix_getv(velMtx, 3, 2) - matrix_getv(Player.Transform, 3, 2))
    )
    Player.Transform *= Matrix.fromEulerAnglesXYZ(0, ry, 0)
    
    CameraPitch += rx
    Camera.Transform = Player.Transform * Matrix.fromTranslation(0, 5, 0) * Matrix.fromEulerAnglesXYZ(CameraPitch, 0, 0)
end

print("done init")