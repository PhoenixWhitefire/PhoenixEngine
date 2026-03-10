local Network = game:GetService("Network")
local Types = require("@ci/test-types.luau")

return function(test: Types.Test, expect: Types.Expect)
    test("Lemongrab", function()
        local response = Network:MakeHttpRequestAsync({ Url = "https://phoenixwhitefire.github.io/net_test.txt" })

        if not response.Ok then
            warn(response.Body)
        end

        expect(response.Ok):toBe(true)
        assert(response.Ok)

        expect(response.Status):toBe(200)
        expect(response.Body):toBe("Lemongrab\n")
    end)
end
