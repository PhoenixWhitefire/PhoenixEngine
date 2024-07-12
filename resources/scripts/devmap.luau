-- devmap.luau, 10/07/2024

local totaltime = 0

newobject("Primitive", "apitest")
setpropvec3("apitest", "Size", 5, 5, 5)

function Update(DeltaTime)
	totaltime += DeltaTime
	setpropvec3("apitest", "Position", math.sin(totaltime) * 15, 10, math.cos(totaltime) * 15)
end