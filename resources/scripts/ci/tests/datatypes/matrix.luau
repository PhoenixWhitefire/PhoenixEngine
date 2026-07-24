local Types = require("@ci/test-types.luau")

return function(test: Types.Test, expect: Types.Expect)
	test("Individual component new", function()
		expect(Matrix.translated(10, 19, 8).Translation):toBe(vector.create(10, 19, 8))
	end)
	test("vector ctor", function()
		expect(Matrix.translated(vector.create(10, 19, 8)).Translation):toBe(vector.create(10, 19, 8))
	end)
	test("identity", function()
		expect(Matrix.new() == Matrix.identity):toBe(true)
	end)

	test("Min bounds", function()
		expect(function()
			local m = Matrix.new()
			local _x = m.C0R1
		end):toThrow()
	end)
	test("Max bounds", function()
		expect(function()
			local m = Matrix.new()
			local _x = m.C5R1
		end):toThrow()
	end)
	test(".new order", function()
		local mtx = Matrix.new(
			 1,  2,  3,  4,
			 5,  6,  7,  8,
			 9, 10, 11, 12,
			13, 14, 15, 16
		)
		expect(mtx.C1R1):toBe(1)
		expect(mtx.C4R1):toBe(4)
		expect(mtx.C4R4):toBe(16)
	end)

	test("ExtractAngles", function()
		local mtx = Matrix.rotated(vector.create(1, 0, 0))
		expect(Matrix.rotated(mtx:ExtractAngles())):toBe(mtx)
	end)
end
