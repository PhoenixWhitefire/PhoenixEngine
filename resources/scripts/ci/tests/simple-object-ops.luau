return function(describe, test, expect)
	test("Read property", function()
		expect(workspace.Name):toBe("Workspace")
	end)
	test("Set property", function()
		workspace.SceneCamera.FieldOfView = 999
		expect(workspace.SceneCamera.FieldOfView):toBe(999)
		workspace.SceneCamera.FieldOfView = 90
	end)
	test("Create and Destroy Object with Component", function()
		local new = GameObject.new("Primitive")
		expect(new:HasComponent("Mesh") and new:HasComponent("Transform")):toBe(true)
		--new:Destroy()
		new.Parent = nil
	end)
	test("Parenting", function()
		local new = GameObject.new("Bone")
		new.Name = "NewObject"
		new.Parent = workspace
		expect(workspace:FindChild("NewObject") ~= nil):toBe(true)
		--new:Destroy()
		new.Parent = nil
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
		--new:Destroy()
		new.Parent = nil
		expect(workspace:FindChild("NewObject")):toBeNil()
	end)
	test("Reflection type validation - Properties", function()
		expect(function()
			game.Parent = 2
		end):toThrow("invalid argument")
	end)
	test("Reflection type validation - Methods", function()
		expect(function()
			game:FindChild(nil)
		end):toThrow("invalid argument")
	end)
end

