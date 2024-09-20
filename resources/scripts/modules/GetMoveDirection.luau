return function()
    local moveX = (if input_keypressed("a") then 1 else 0) - (if input_keypressed("d") then 1 else 0)
    local moveY = (if input_keypressed("e") then 1 else 0) - (if input_keypressed("q") then 1 else 0)
    local moveZ = (if input_keypressed("w") then 1 else 0) - (if input_keypressed("s") then 1 else 0)
    
    return Vector3.new(moveX, moveY, moveZ)
    
    --[[
    local transrights = Camera.Transform
    
    matrix_setv(transrights, 3, 0, matrix_getv(transrights, 3, 0) + (moveX * speed))
    matrix_setv(transrights, 3, 1, matrix_getv(transrights, 3, 1) + (moveY * speed))
    matrix_setv(transrights, 3, 2, matrix_getv(transrights, 3, 2) + (moveZ * speed))
    
    Camera.Transform = transrights -- We give the Camera some rights (this means `Object_Camera` is canonically trans)
    
    ]]--
end