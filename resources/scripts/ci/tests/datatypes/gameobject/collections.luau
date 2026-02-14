local Collections: Collections = game:GetService("Collections") :: any
local Types = require("@ci/test-types.luau")

return function(test: Types.Test, expect: Types.Expect)
    test("GetTagged", function()
        game:AddTag("Test1")
        expect(Collections:GetTagged("Test1")[1]):toBe(game)
    end)
    test("HasTag", function()
        game:AddTag("Test2")
        expect(game:HasTag("Test2")):toBe(true)
        game:RemoveTag("Test2")
        expect(game:HasTag("Test2")):toBe(false)
    end)
    test("TagAdded", function()
        local t1added = false
        local t3added = false
        local t1addedobj = false
        local t3addedobj = false

        local c1 = Collections:GetTagAddedSignal("Test1"):Connect(function(g)
            expect(g):toBe(game)
            expect(t1added):toBe(false)
            t1added = true
        end)
        local c2 = Collections:GetTagAddedSignal("Test3"):Connect(function(g)
            expect(g):toBe(game)
            expect(t3added):toBe(false)
            t3added = true
        end)
        local c3 = game.OnTagAdded:Connect(function(t)
            if t == "Test1" then
                t1addedobj = true
            else
                expect(t):toBe("Test3" :: "Test3")
                t3addedobj =true
            end
        end)

        game:AddTag("Test1") -- already have this tag
        game:AddTag("Test3")

        task.wait()
        expect(t1added):toBe(false)
        expect(t3added):toBe(true)
        expect(t1addedobj):toBe(false)
        expect(t3addedobj):toBe(true)

        c1:Disconnect()
        c2:Disconnect()
        c3:Disconnect()
    end)
    test("TagRemoved", function()
        local t3removed = false
        local t3removedobj = false

        local c1 = Collections:GetTagRemovedSignal("Test3"):Connect(function(g)
            expect(g):toBe(game)
            expect(t3removed):toBe(false)
            t3removed = true
        end)
        local c2 = game.OnTagRemoved:Connect(function(t)
            expect(t):toBe("Test3")
            expect(t3removedobj):toBe(false)
            t3removedobj = true
        end)

        game:RemoveTag("Test3")

        task.wait()
        expect(t3removed):toBe(true)
        expect(t3removedobj):toBe(true)

        c1:Disconnect()
        c2:Disconnect()
    end)
    test("GetTags", function()
        local collections = Collections:GetCollections()
        expect(#collections):toBe(3)

        expect(table.find(collections, "Test1")).never:toBeNil()
        expect(table.find(collections, "Test2")).never:toBeNil()
        expect(table.find(collections, "Test3")).never:toBeNil()
    end)
    test("No tags", function()
        game:RemoveTag("Test1")
        game:RemoveTag("Test9000")
        expect(game:HasTag("Test1")):toBe(false)
        expect(game:GetTags()):toBe({})
    end)
    test("Destroy removes tags", function()
        local t4removed = false
        local t4removedobj = false

        local o = GameObject.new("Mesh")

        local c1 = Collections:GetTagRemovedSignal("Test4"):Connect(function(g)
            expect(g):toBe(o)
            expect(t4removed):toBe(false)
            t4removed = true
        end)

        local c2 = o.OnTagRemoved:Connect(function(t)
            expect(t):toBe("Test4")
            expect(t4removedobj):toBe(false)
            t4removedobj = true
        end)

        o:AddTag("Test4")
        o:Destroy()

        task.wait()

        expect(t4removed):toBe(true)
        expect(t4removedobj):toBe(true)

        c1:Disconnect()
        c2:Disconnect()
    end)
end
