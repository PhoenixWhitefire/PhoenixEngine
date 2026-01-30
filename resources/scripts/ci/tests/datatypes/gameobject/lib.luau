return function(describe, test, expect)
    test("new", function()
		local new = GameObject.new("SpotLight", "Animation")
		expect(new:HasComponent("SpotLight") and new:HasComponent("Transform") and new:HasComponent("Animation")):toBe(true)
		new:Destroy()
	end)
    test("fromId", function()
        expect(GameObject.fromId(game.ObjectId)):toBe(game)
    end)
    test("validComponents", function()
        expect(#GameObject.validComponents > 5):toBe(true)
        for i, v in GameObject.validComponents do
            expect(i > 0):toBe(true)
            expect(typeof(v) == "string")
        end
    end)
end
