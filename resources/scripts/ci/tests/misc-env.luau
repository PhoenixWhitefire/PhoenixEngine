return function(describe, test, expect)
	test("Sleep", function()
		expect(function()
			sleep(0)
		end).never:toThrow()
	end)
	
	test("Env Sandboxing", function()
		string = nil
		expect(string).never:toBeNil()
	end)
end

