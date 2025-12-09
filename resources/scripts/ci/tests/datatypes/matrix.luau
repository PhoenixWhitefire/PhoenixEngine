return function(describe, test, expect)
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
		local mtx = Matrix.fromAngles(vector.create(1, 0, 0))
		expect(Matrix.fromAngles(mtx:ExtractAngles())):toBe(mtx)
	end)
end
