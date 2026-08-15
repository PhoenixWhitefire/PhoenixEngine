local Types = require("@ci/test-types.luau")

return function(test: Types.Test, expect: Types.Expect)
    test("Read property", function()
        expect(workspace.Name):toBe("Workspace")
    end)
    test("Set property", function()
        workspace.SceneCamera.FieldOfView = 999
        expect(workspace.SceneCamera.FieldOfView):toBe(999)
        workspace.SceneCamera.FieldOfView = 90
    end)
    test("Parenting", function()
        local new = GameObject.fromTemplate("Bone")
        new.Name = "NewObject"
        new.Parent = workspace
        expect(workspace:FindChild("NewObject") ~= nil):toBe(true)
        new:Destroy()
    end)
    test("Un-Parent", function()
        local new = GameObject.fromTemplate("Transform")
        new.Name = "NewObject"
        new.Parent = workspace
        new.Parent = nil
        expect(workspace:FindChild("NewObject")):toBeNil()
    end)
    test("Un-Parent via :Destroy", function()
        local new = GameObject.fromTemplate("DataModel")
        new.Name = "NewObject"
        new.Parent = workspace
        new:Destroy()
        expect(workspace:FindChild("NewObject")):toBeNil()
    end)
    test("Type validation - Properties", function()
        expect(function()
            game.Parent = 2
        end):toThrow("invalid argument")
    end)
    test("Type validation - Methods", function()
        expect(function()
            game:FindChild(nil)
        end):toThrow("invalid argument")
    end)
    test("__eq", function()
        expect(game.Workspace == workspace and game.Workspace ~= game):toBe(true)
    end)
    test("rawequal true", function()
        expect((function()
            local t = {
                [game.Workspace] = false
            }
            t[game.Workspace] = true
            return t[game.Workspace]
        end)()):toBe(true)
    end)
    test("rawequal false", function()
        expect((function()
            local t = {
                [game.Workspace] = false
            }
            t[workspace.SceneCamera] = true
            return t[game.Workspace]
        end)()):toBe(false)
    end)

    test("Non-namecall method calls work", function()
        local o = GameObject.new()
        o.AddTag(o, "Hello")
        expect(o:GetTags()):toBe({ "Hello" })
        o:Destroy()
    end)

    test("Non-namecall method call equality", function()
        expect(game.Close):toBe(game.Close)
        expect(rawequal(game.Close, game.Close)):toBe(true)
    end)

    test("Errorneous non-namecall method call", function()
        expect(function()
            game.Close(workspace)
        end):toThrow("Close not available on DataModel.Workspace")
    end)
end
