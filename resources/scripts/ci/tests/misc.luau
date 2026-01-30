return function(describe, test, expect)
	test("Env Sandboxing", function()
		string = nil
		expect(string).never:toBeNil()
	end)
end

