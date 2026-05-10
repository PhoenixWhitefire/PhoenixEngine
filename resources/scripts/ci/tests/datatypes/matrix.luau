local Types = require("@ci/test-types.luau")

return function(test: Types.Test, expect: Types.Expect)
	test("Individual component new", function()
		expect(Matrix.translated(10, 19, 8).Position):toBe(vector.create(10, 19, 8))
	end)
	test("vector ctor", function()
		expect(Matrix.translated(vector.create(10, 19, 8)).Position):toBe(vector.create(10, 19, 8))
	end)
	test("identity", function()
		expect(Matrix.new() == Matrix.identity):toBe(true)
	end)

	test("rotatedYXZ alias", function()
		expect(Matrix.rotated(0, math.rad(10), math.rad(19))):toBe(Matrix.rotatedYXZ(0, math.rad(10), math.rad(19)))
	end)

	test("Min bounds", function()
		expect(function()
			local m = Matrix.new()
			m.C0R1 = m.C0R1
		end):toThrow()
	end)
	test("Max bounds", function()
		expect(function()
			local m = Matrix.new()
			m.C5R1 = m.C5R1
		end):toThrow()
	end)
	test("Normal edge assign", function()
		expect(function()
			local m = Matrix.new()
			m.C1R4 = 5
			assert(m.C1R4 == 5)
		end).never:toThrow()
	end)

	test("ExtractAngles", function()
		local mtx = Matrix.rotated(vector.create(1, 0, 0))
		expect(Matrix.rotated(mtx:ExtractAngles())):toBe(mtx)
	end)
end
