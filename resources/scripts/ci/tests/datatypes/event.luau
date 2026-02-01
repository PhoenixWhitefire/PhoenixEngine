local Types = require("@ci/test-types.luau")

return function(test: Types.Test, expect: Types.Expect)
    test("Connect and disconnect", function()
        local count = 0
        local function func()
            count += 1
        end

        local connection = nil :: EventConnection?

        expect(function()
            connection = game.OnFrameBegin:Connect(func)
        end).never:toThrow()

        game.OnFrameBegin:WaitUntil()
        task.wait() -- We might be earlier in the resumption queue, so wait until the next resumption point
        expect(count):toBe(1)

        assert(connection)
        connection:Disconnect()

        game.OnFrameBegin:WaitUntil()
        task.wait()
        expect(count):toBe(1)
    end)
    test("WaitUntil", function()
        local n = game.OnFrameBegin:WaitUntil()
        expect(typeof(n)):toBe("number")
        expect(n > 0):toBe(true)
    end)
    test(".Connected", function()
        local conn = game.OnFrameBegin:Connect(function() end)
        expect(conn.Connected):toBe(true)
        conn:Disconnect()
        expect(conn.Connected):toBe(false)
        expect(function()
            conn:Disconnect()
        end):toThrow("already ")
    end)
    test(".Signal", function()
        local conn = game.OnFrameBegin:Connect(function() end)
        expect(conn.Signal):toBe(game.OnFrameBegin)
        conn:Disconnect()
    end)
end
