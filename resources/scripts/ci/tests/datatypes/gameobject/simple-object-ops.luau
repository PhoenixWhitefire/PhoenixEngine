local Workspace = game:GetService("Workspace")

local Types = require("@ci/test-types.luau")

return function(test: Types.Test, expect: Types.Expect)
    test("Read property", function()
        expect(Workspace.Name):toBe("Workspace")
    end)
    test("Set property", function()
        Workspace.SceneCamera.FieldOfView = 999
        expect(Workspace.SceneCamera.FieldOfView):toBe(999)
        Workspace.SceneCamera.FieldOfView = 90
    end)
    test("Parenting", function()
        local new = GameObject.fromTemplate("Bone")
        new.Name = "NewObject"
        new.Parent = Workspace
        expect(Workspace:FindChild("NewObject") ~= nil):toBe(true)
        new:Destroy()
    end)
    test("Un-Parent", function()
        local new = GameObject.fromTemplate("Transform")
        new.Name = "NewObject"
        new.Parent = Workspace
        new.Parent = nil
        expect(Workspace:FindChild("NewObject")):toBeNil()
    end)
    test("Un-Parent via :Destroy", function()
        local new = GameObject.fromTemplate("DataModel")
        new.Name = "NewObject"
        new.Parent = Workspace
        new:Destroy()
        expect(Workspace:FindChild("NewObject")):toBeNil()
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
        expect(game.Workspace == Workspace and game.Workspace ~= game):toBe(true)
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
            t[Workspace.SceneCamera] = true
            return t[game.Workspace]
        end)()):toBe(false)
    end)

    test("Non-namecall method calls work", function()
        local o = GameObject.new()
        o.AddComponent(o, "Transform")
        expect(o:GetComponents()):toBe({ "Transform" })
        o:Destroy()
    end)

    test("Non-namecall method call equality", function()
        expect(game.Close):toBe(game.Close)
        expect(rawequal(game.Close, game.Close)):toBe(true)
    end)

    test("Errorneous non-namecall method call", function()
        expect(function()
            game.Close(Workspace)
        end):toThrow("Close not available on DataModel.Workspace")
    end)
end
