--!optimize 0

return function(describe, test, expect)
    test("wait", function()
		expect(function()
			task.wait(0)
		end).never:toThrow()
	end)

    test("defer function", function()
        debug.breakpoint()

        local function deferTest()
            task.defer(function(c: thread, n)
                coroutine.resume(c, n)
            end, coroutine.running(), 57)

            return coroutine.yield()
        end

        expect(deferTest()):toBe(57)
    end)

    test("defer", function()
        local function deferTest()
            task.defer(coroutine.running(), 42)
            return coroutine.yield()
        end

        expect(deferTest()):toBe(42)
    end)

    test("delay", function()
        local function delayTest()
            task.delay(0, coroutine.running(), 64)
            return coroutine.yield()
        end

        expect(delayTest()):toBe(64)
    end)
end
