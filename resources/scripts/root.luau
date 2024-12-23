-- root.luau, 21/12/2024

local ModelImporterPath = "models/EmbeddedTexture.glb"
local ModelImporterScale = 0.1

local ScriptExecutionFile = "scripts/shdgraphtest.luau"

function Update()
	if imgui_begin("Model Importer") then
		if imgui_textlink("Model: " .. ModelImporterPath) then
			imfd_open("Select Model", 0, "resources/models", "*.gltf,*.glb")
		end
		
		ModelImporterScale = imgui_input_float("Scale", ModelImporterScale)
		
		if imgui_button("Import") then
			local pathSplit = string.split(ModelImporterPath, "/")
			
			local parts = model_import(ModelImporterPath)
			local model = GameObject.new("Model")
			model.Name = pathSplit[#pathSplit]
			model.Parent = workspace
			
			for _, part in parts do
				part.Size *= ModelImporterScale
				part.Parent = model
			end
		end
	end
	imgui_end()
	
	if imfd_begin("Select Model") then
		if imfd_actiondone() then
			if imfd_selectionmade() then
				local selection = imfd_getselections()[1]
				local resDirOff = selection:find("resources/")
				if not resDirOff then
					error("Must be within resources directory!")
				else
					ModelImporterPath = selection:sub(resDirOff + 10, resDirOff + (#selection - resDirOff))
				end
			end
			
			imfd_close()
		end
		
		imfd_end()
	end
	
	if imgui_begin("Run Script") then
		if imgui_textlink("Path: " .. ScriptExecutionFile) then
			imfd_open("Script Select Dialog", 0, "resources/scripts", "*.luau")
		end
		
		if imgui_button("Execute") then
			if scriptHost then
				scriptHost:Destroy()
			end
			scriptHost = GameObject.new("Script")
			scriptHost.Name = "TempScriptHost"
			scriptHost.SourceFile = ScriptExecutionFile
			scriptHost.Parent = script
		end
	end
	imgui_end()
	
	if imfd_begin("Script Select Dialog") then
		if imfd_actiondone() then
			if imfd_selectionmade() then
				local selection = imfd_getselections()[1]
				local resDirOff = selection:find("resources/")
				if not resDirOff then
					error("Must be within resources directory!")
				else
					ScriptExecutionFile = selection:sub(resDirOff + 10, resDirOff + (#selection - resDirOff))
				end
			end
			
			imfd_close()
		end
		
		imfd_end()
	end
	
	imgui_begin("File")
	
	if imgui_button("Save WORLD") then
		scene_save({ game }, "scenes/root.world")
	end
	
	FileSaveSubLevel = imgui_input_text("Save Sub-Level Target", FileSaveSubLevel or "scenes/rooms.hxscene")
	
	if imgui_button("Save Sub-Level") then
		scene_save({ workspace.Level }, FileSaveSubLevel)
	end
	
	FileLoadSubLevel = imgui_input_text("Load Sub-Level Target", FileLoadSubLevel or "scenes/rooms.hxscene")
	
	if imgui_button("Load Sub-Level") then
		workspace.Level:Destroy()
		
		local subLevelContainer = GameObject.new("Model")
		subLevelContainer.Name = "Level"
		subLevelContainer.Parent = workspace
		
		for _, root in scene_load(FileLoadSubLevel) do
			root.Parent = subLevelContainer
		end
	end
	
	imgui_end()
end