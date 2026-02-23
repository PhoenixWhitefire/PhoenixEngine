local Network: Network = game:GetService("Network") :: any
local Types = require("@ci/test-types.luau")

return function(test: Types.Test, expect: Types.Expect)
    test("Lemongrab", function()
        expect(Network:MakeHttpRequestAsync("https://phoenixwhitefire.github.io/net_test.txt")):toBe({ Ok = true, Status = 200, Body = "Lemongrab\n" })
    end)
end
