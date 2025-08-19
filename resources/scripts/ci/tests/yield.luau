return function(describe, test, expect)
	test("Sleep", function()
		expect(function()
			sleep(0)
		end).never:toThrow()
	end)
end
