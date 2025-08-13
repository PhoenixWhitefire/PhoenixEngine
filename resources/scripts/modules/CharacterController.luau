-- modules/character-controller.luau, 23/09/2024

local Controller = {}
Controller.__index = Controller

function Controller.new(Camera)
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
	}, Controller)
end

function Controller:InitDefault(CharacterHeight)
	CharacterHeight = CharacterHeight or 15
	
	local character = GameObject.new("Primitive")
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

local CHARACTER_MAX_STEP_UP_DIST = 1.5

function Controller:Update(DeltaTime)
	self.totalTime += DeltaTime
	
	local moveDirection, inputDirection = self:getMoveDirection()
	local rotationX, rotationY = self:getRotationVector()
	
	local charSize = self.Character.Size
	
	local floorRayHit = world_aabbcast(
		self.Character.Transform.Position - vector.create(0, charSize.Y / 4, 0),
		vector.create(charSize.x * 1.2, charSize.Y / 2, charSize.z * 1.2),
		{ self.Character }
	)
	
	local impulse = vector.create(
		moveDirection.X * self.Acceleration * DeltaTime,
		0,
		moveDirection.Z * self.Acceleration * DeltaTime
	)
	
	if floorRayHit then
		if input_keypressed(" ") and (self.totalTime - self.lastJumped) > self.JumpCooldown then
			impulse += vector.create(0, self.JumpImpulse, 0)
			self.Character.LinearVelocity = vector.create(self.Character.LinearVelocity.X, 0, self.Character.LinearVelocity.Z)
			self.lastJumped = self.totalTime
		end
		
		local sink = (self.Character.Transform.Position.Y - charSize.Y / 2) - floorRayHit.Position.Y
		
		if sink > 0.2 and vector.magnitude(moveDirection) > 0 then
			self.Character.Transform *= Matrix.fromTranslation(0, sink, 0)
		end
	else
		impulse *= 0.3
	end
	
	local currentVelocity = self.Character.LinearVelocity
	self.Character.LinearVelocity = currentVelocity + impulse
	
	self.Character.Transform *= Matrix.fromEulerAnglesXYZ(0, rotationY, 0)
	
	self.cameraPitch += rotationX
	self.cameraPitch = math.clamp(self.cameraPitch, -math.rad(89), math.rad(89))
	
	local camTransform
	
	local rollAdd = -vector.dot(vector.normalize(moveDirection), self.Camera.Transform.Right) * DeltaTime * 0.5
	--rollAdd = moveDirection.X * 0.05
	if rollAdd == rollAdd then
		self.roll += rollAdd
	end

	self.roll -= DeltaTime * self.roll

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

function Controller:getMoveDirection()
	local moveX = (if input_keypressed("a") then 1 else 0) - (if input_keypressed("d") then 1 else 0)
	local moveZ = (if input_keypressed("w") then 1 else 0) - (if input_keypressed("s") then 1 else 0)
	
	local trans = self.Camera.Transform * Matrix.fromEulerAnglesXYZ(-self.cameraPitch, 0, 0) * Matrix.fromTranslation(moveX, 0, moveZ)
	
	return vector.create(
		matrix_getv(trans, 3, 0) - matrix_getv(self.Camera.Transform, 3, 0),
		0,
		matrix_getv(trans, 3, 2) - matrix_getv(self.Camera.Transform, 3, 2)
	), vector.create(moveX, 0, moveZ)
	
	--[[
	local transrights = Camera.Transform
	
	matrix_setv(transrights, 3, 0, matrix_getv(transrights, 3, 0) + (moveX * speed))
	matrix_setv(transrights, 3, 1, matrix_getv(transrights, 3, 1) + (moveY * speed))
	matrix_setv(transrights, 3, 2, matrix_getv(transrights, 3, 2) + (moveZ * speed))
	
	Camera.Transform = transrights -- We give the Camera some rights (this means `Object_Camera` is canonically trans)
	
	]]--
end

function Controller:getRotationVector()
	if input_mouse_bdown(true) then
		self.isMouseLocked = true -- lock mouse with left-click
		
	elseif input_mouse_bdown(false) then
		self.isMouseLocked = false -- unlock mouse with right-click
	end
	input_mouse_setlocked(self.isMouseLocked)
	
	local rx, ry = 0, 0
	local mouseX, mouseY = input_mouse_getpos()
	
	if self.isMouseLocked then
		rx, ry = (mouseY - self.prevMouseY) / 400, (self.prevMouseX - mouseX) / 400
		-- when the mouse is warped to the other side of the screen upon hitting the edge,
		-- when it's locked to window
		-- 23/09/2024
		if math.abs(rx) > math.rad(45) then
			rx = 0
		end
		if math.abs(ry) > math.rad(45) then
			ry = 0
		end
	end
	
	self.prevMouseX, self.prevMouseY = mouseX, mouseY
	
	return rx, ry
end

return Controller

