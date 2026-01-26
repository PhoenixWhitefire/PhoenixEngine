return function(describe, test: (string, () -> ()) -> (), expect: (any) -> (any))
	test("Read property", function()
		expect(workspace.Name):toBe("Workspace")
	end)
	test("Set property", function()
		workspace.SceneCamera.FieldOfView = 999
		expect(workspace.SceneCamera.FieldOfView):toBe(999)
		workspace.SceneCamera.FieldOfView = 90
	end)
	test("Component check", function()
		local new = GameObject.new("SpotLight")
		expect(new:HasComponent("SpotLight") and new:HasComponent("Transform")):toBe(true)
		new:Destroy()
	end)
	test("Parenting", function()
		local new = GameObject.new("Bone")
		new.Name = "NewObject"
		new.Parent = workspace
		expect(workspace:FindChild("NewObject") ~= nil):toBe(true)
		new:Destroy()
	end)
	test("Un-Parent", function()
		local new = GameObject.new("Bone")
		new.Name = "NewObject"
		new.Parent = workspace
		new.Parent = nil
		expect(workspace:FindChild("NewObject")):toBeNil()
	end)
	test("Un-Parent via :Destroy", function()
		local new = GameObject.new("Bone")
		new.Name = "NewObject"
		new.Parent = workspace
		new:Destroy()
		expect(workspace:FindChild("NewObject")):toBeNil()
	end)
	test(".Exists", function()
		local new = GameObject.new("Model")
		expect(new.Exists):toBe(true)
		new:Destroy()
		expect(new.Exists):toBe(false)
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
	test("__eq True", function()
		expect((function()
			local t = {
				[game.Workspace] = false
			}
			t[game.Workspace] = true
			return t[game.Workspace]
		end)()):toBe(true)
	end)
	test("__eq False", function()
		expect((function()
			local t = {
				[game.Workspace] = false
			}
			t[workspace.SceneCamera] = true
			return t[game.Workspace]
		end)()):toBe(false)
	end)
end

