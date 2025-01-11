-- root.luau, 21/12/2024

local NodeEditor = require("scripts/modules/NodeEditor.luau")
local Editor = function() end--require("scripts/modules/Editor.luau")

local ModelImporterScale = 1

local ScriptExecutionFile = "scripts/shdgraphtest.luau"

local DefaultSubLevel = "scenes/dev_fmtv2.hxscene"

local function imfdOnSelectedCallback(DialogName, Callback)
	if imfd_begin(DialogName) then
		if imfd_actiondone() then
			if imfd_selectionmade() then
				local selection = imfd_getselections()[1]
				if (not selection) then
					Callback(imfd_getrawresult())
				else
					local resDirOff = selection:find("resources/")
					if not resDirOff then
						error("Must be within resources directory!")
					else
						Callback(selection:sub(resDirOff + 10, resDirOff + (#selection - resDirOff)))
					end
				end
			end
			
			imfd_close()
		end
		
		imfd_end()
	end
end

local function imfdOnSelectedPathOverride(DialogName, CurrentPath)
	local path = CurrentPath
	
	imfdOnSelectedCallback(DialogName, function(newPath)
		path = newPath
	end)
	
	return path
end

local function loadSubLevel(Path)
	for _, v in workspace.Level:GetChildren() do
		v:Destroy()
	end
	
	for _, root in scene_load(Path) do
		root.Parent = workspace.Level
	end
end

function Update()
	if imgui_begin("Model Importer") then
		ModelImporterScale = imgui_input_float("Scale", ModelImporterScale)

		if imgui_button("Select & Import Model") then
			imfd_open("ImportModel", 0, "resources/models", "*.gltf,*.glb")
		end
	end
	imgui_end()
	
	imfdOnSelectedCallback("ImportModel", function(Path)
		local pathSplit = string.split(Path, "/")
		
		local parts = model_import(Path)
		--local model = GameObject.new("Model")
		--model.Name = pathSplit[#pathSplit]
		--model.Parent = workspace
		
		--for _, part in parts do
			--part.Size *= ModelImporterScale
		--	part.Parent = workspace
		--end
		
		parts.Parent = workspace
	end)
	
	if imgui_begin("Run Script") then
		if imgui_textlink("Path: " .. ScriptExecutionFile) then
			imfd_open("SelectScript", 0, "resources/scripts", "*.luau")
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
	
	ScriptExecutionFile = imfdOnSelectedPathOverride("SelectScript", ScriptExecutionFile)
	
	imgui_begin("File")
	
	if imgui_button("Save World") then
		imfd_open("SaveWorld", 1, "resources/scenes/", "*.world")
		IntendedSaveWorldState = game:Duplicate()
		IntendedSaveWorldState.Parent = nil
	end
	
	imfdOnSelectedCallback("SaveWorld", function(Path)
		if not Path:find(".world") then
			Path = `scenes/{ Path }.world`
		end
		scene_save({ IntendedSaveWorldState }, Path)
		IntendedSaveWorldState:Destroy()
	end)
	
	local loadWorld = imgui_button("Load World")

	if loadWorld then
		imfd_open("LoadWorld", 0, "resources/scenes/", "*.world")
	end
	
	imfdOnSelectedCallback("LoadWorld", function(Path)
		local dm = scene_load(Path)[1]
		if not dm then
			error("No root object!")
		end
		
		for _, v in game:GetChildren() do
			v:Destroy()
		end
		
		game:MergeWith(dm)
	end)
	
	if imgui_button("Save Sub-Level") then
		IntendedLevelState = workspace.Level:Duplicate()
		IntendedLevelState.Parent = nil
		imfd_open("SaveSubLevel", 1, "resources/scenes/", "*.hxscene")
	end
	
	imfdOnSelectedCallback("SaveSubLevel", function(Path)
		if not Path:find(".hxscene") then
			Path = `scenes/{ Path }.hxscene`
		end
		scene_save(IntendedLevelState:GetChildren(), Path)
		IntendedLevelState:Destroy()
	end)
	
	if imgui_button("Load Sub-Level") then
		imfd_open("LoadSubLevel", 0, "resources/scenes/", "*.hxscene")
	end
	
	imfdOnSelectedCallback("LoadSubLevel", loadSubLevel)
	
	imgui_end()
	
	Editor(DeltaTime)
	--NodeEditor()
end

loadSubLevel(DefaultSubLevel)

if not conf_get("Developer") then
	Update = nil
end
