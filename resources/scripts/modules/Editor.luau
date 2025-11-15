-- NO LONGER USED!!!!!! 15/11/2025

-- Editor.luau, 27/12/2024

local LuaEditorEnabled = false

local CurrentSelection = nil
local MouseWasDownLastFrame = false

local mouseRayResult = {}
local prevMouseRayResultPos = nil

local function insertObject()
	local new = GameObject.new("Mesh" :: "Mesh")
	new.Parent = workspace.Level
	CurrentSelection = new
end

local function handleMouseSelection()
	if not input_mouse_bdown(true) then
		MouseWasDownLastFrame = false
		return
		
	elseif MouseWasDownLastFrame then
		return
	end

	MouseWasDownLastFrame = true
	
	local origin = game.Workspace.Camera.Transform.Position
	
	mouseRayResult = world_raycast(origin, mouseRayVec, {})
	CurrentSelection = if mouseRayResult then mouseRayResult.Object else nil
end

local function updateSelectionAdorns()
	if not CurrentSelection then
		return
	end
	
	if input_mouse_bdown(true) then
		local dragRayResult = world_raycast(game.Workspace.SceneCamera.Transform.Position, mouseRayVec, { CurrentSelection })
		
		if dragRayResult then
			CurrentSelection.Transform = Matrix.fromTranslation(dragRayResult.Position + dragRayResult.Normal * CurrentSelection.Size / 2)
		end
	end
	
	prevMouseRayResultPos = if mouseRayResult then mouseRayResult.Position else nil
end

return function(DeltaTime)
	imgui_begin("L.Editor")
	
	LuaEditorEnabled = imgui_checkbox("Enabled", LuaEditorEnabled)
	
	imgui_end()
	
	if not LuaEditorEnabled then
		return
	end
	
	if input_keypressed("h") then
		insertObject()
	end
	
	local mouseX, mouseY = input_mouse_getpos()
	mouseRayVec = workspace:ScreenPointToRay({ mouseX, mouseY }, 100)
	mouseRayVec *= 100
	
	handleMouseSelection()
	updateSelectionAdorns()
end