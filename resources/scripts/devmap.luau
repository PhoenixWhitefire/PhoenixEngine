-- devmap.luau, 10/07/2024

local totaltime = 0

--local v3 = Vector3.new(5, 10, 3)

--for k, v in v3 do
--	print(k, "=", v)
--end

print("NIKO")

local cube = GameObject.new("Primitive")

print("HI")

function Update(DeltaTime)
	totaltime += DeltaTime
	print("PINT")
	local nvec3 = Vector3.new(math.sin(totaltime) * 15, 10, math.cos(totaltime) * 15)
	print("BRUH")
	cube:SetPosition(nvec3)
	print("SETPOS")
end