-- modules/character-controller.luau, 23/09/2024
local InputService: InputService = game:GetService("InputService") :: any

local Controller = {}
Controller.__index = Controller

type ControllerData = {
	Character: GameObject & Mesh,
	Camera: GameObject & Camera,

	Acceleration: number,
	JumpImpulse: number,
	JumpCooldown: number,
	MouseSensitivity: number,
	JumpRayDistance: number,

	FirstPerson: boolean,
	CameraOffset: Matrix,
	CameraTargetOffset: vector,

	prevMouseX: number,
	prevMouseY: number,
	isMouseLocked: boolean,
	totalTime: number,
	lastJumped: number,
	cameraPitch: number,
	roll: number
}

export type Controller = setmetatable<ControllerData, typeof(Controller)>

function Controller.new(Camera: GameObject & Camera): Controller
	return setmetatable({
		Camera = Camera,
		
		Acceleration = 240,
		JumpImpulse = 25,
		JumpCooldown = 0.5,
		MouseSensitivity = 60,
		JumpRayDistance = 8,
		
		FirstPerson = true,
		CameraOffset = Matrix.fromTranslation(0, 0, 0),
		-- relative to the character, where to look when in third-person
		CameraTargetOffset = vector.zero,
		
		prevMouseX = 0,
		prevMouseY = 0,
		isMouseLocked = false,
		totalTime = 0,
		lastJumped = 0,
		cameraPitch = 0,
		roll = 0
	}, Controller) :: any
end

function Controller.InitDefault(self: Controller, CharacterHeight)
	CharacterHeight = CharacterHeight or 15
	
	local character = GameObject.new("Mesh" :: "Mesh")
	character.Size = vector.create(CharacterHeight / 3, CharacterHeight, CharacterHeight / 3)
	character.PhysicsDynamics = true
	character.Friction = 4
	character.CollisionFidelity = 1 -- AabbStaticSize
	character.Parent = workspace
	
	self.Character = character
	self.Camera.UseSimpleController = false
	self.CameraOffset = Matrix.fromTranslation(0, CharacterHeight * 0.4, 0)
	self.JumpRayDistance = CharacterHeight * 0.6
end

function Controller.Update(self: Controller, DeltaTime: number)
	self.totalTime += DeltaTime
	
	local moveDirection, _ = self:getMoveDirection()
	local rotationX, rotationY = self:getRotationVector()
	
	local charSize = self.Character.Size
	
	local floorRayHit = #workspace:GetObjectsInAabb(
		self.Character.Transform.Position - vector.create(0, charSize.Y / 4, 0),
		vector.create(charSize.x * 1.2, charSize.Y / 2, charSize.z * 1.2),
		{ self.Character }
	) > 0
	
	local impulse = vector.create(
		moveDirection.x * self.Acceleration * DeltaTime,
		0,
		moveDirection.z * self.Acceleration * DeltaTime
	)
	
	if floorRayHit then
		if InputService:IsKeyPressed(Enum.Key.Space) and (self.totalTime - self.lastJumped) > self.JumpCooldown then
			impulse += vector.create(0, self.JumpImpulse, 0)
			self.Character.LinearVelocity = vector.create(self.Character.LinearVelocity.x, 0, self.Character.LinearVelocity.z)
			self.lastJumped = self.totalTime
		end
	else
		impulse *= 0.3
	end
	
	local currentVelocity = self.Character.LinearVelocity
	self.Character.LinearVelocity = currentVelocity + impulse
	
	self.Character.Transform *= Matrix.fromEulerAnglesXYZ(0, rotationY, 0)
	
	self.cameraPitch += rotationX
	self.cameraPitch = math.clamp(self.cameraPitch, -math.rad(89), math.rad(89))
	
	local rollAdd = -vector.dot(vector.normalize(moveDirection), self.Camera.Transform.Right) * DeltaTime * 0.5
	--rollAdd = moveDirection.X * 0.05
	if rollAdd == rollAdd then
		self.roll += rollAdd
	end

	self.roll -= DeltaTime * self.roll

	local camTransform

	if self.FirstPerson then
		camTransform = self.Character.Transform
						* self.CameraOffset
						* Matrix.fromEulerAnglesXYZ(self.cameraPitch, 0, self.roll)
	else
		camTransform = self.Character.Transform
						* Matrix.fromEulerAnglesXYZ(self.cameraPitch, 0, 0)
						* self.CameraOffset
						* Matrix.fromEulerAnglesXYZ(0, 0, self.roll)
	end
	
	self.Camera.Transform = camTransform
end

function Controller.getMoveDirection(self: Controller)
	local moveX = (if InputService:IsKeyPressed(Enum.Key.A) then 1 else 0) - (if InputService:IsKeyPressed(Enum.Key.D) then 1 else 0)
	local moveZ = (if InputService:IsKeyPressed(Enum.Key.W) then 1 else 0) - (if InputService:IsKeyPressed(Enum.Key.S) then 1 else 0)
	
	local trans = self.Camera.Transform * Matrix.fromEulerAnglesXYZ(-self.cameraPitch, 0, 0) * Matrix.fromTranslation(moveX, 0, moveZ)
	
	return vector.create(
		trans.C4R1 - self.Camera.Transform.C4R1,
		0,
		trans.C4R3 - self.Camera.Transform.C4R3
	), vector.create(moveX, 0, moveZ)
	
	--[[
	local transrights = Camera.Transform
	
	matrix_setv(transrights, 3, 0, matrix_getv(transrights, 3, 0) + (moveX * speed))
	matrix_setv(transrights, 3, 1, matrix_getv(transrights, 3, 1) + (moveY * speed))
	matrix_setv(transrights, 3, 2, matrix_getv(transrights, 3, 2) + (moveZ * speed))
	
	Camera.Transform = transrights -- We give the Camera some rights (this means `Object_Camera` is canonically trans)
	
	]]--
end

function Controller.getRotationVector(self: Controller) : (number, number)
	if InputService:IsMouseButtonPressed(Enum.MouseButton.Left) and not InputService.IsMouseSunk then
		self.isMouseLocked = true -- lock mouse with left-click
		
	elseif InputService:IsMouseButtonPressed(Enum.MouseButton.Right) then
		self.isMouseLocked = false -- unlock mouse with right-click
	end
	InputService.CursorMode = if self.isMouseLocked then Enum.CursorMode.Disabled else Enum.CursorMode.Normal
	
	local rx, ry = 0, 0
	local cursor = InputService:GetCursorPosition()
	
	if self.isMouseLocked then
		rx, ry = (cursor.y - self.prevMouseY) / 400, (self.prevMouseX - cursor.x) / 400
	end
	
	self.prevMouseX, self.prevMouseY = cursor.x, cursor.y
	
	return rx, ry
end

return Controller

