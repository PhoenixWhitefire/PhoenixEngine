local Types = require("@ci/test-types.luau")

return function(test: Types.Test, expect: Types.Expect)
	test("Env Sandboxing", function()
		string = nil
		expect(string).never:toBeNil()
	end)
end
