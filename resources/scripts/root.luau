-- root.luau, 21/12/2024

local NodeEditor = nil --require("scripts/modules/NodeEditor.luau")
local Editor = function() end--require("scripts/modules/Editor.luau")

local ModelImporterScale = 1

local ScriptExecutionFile = "scripts/mc.luau"

local DefaultSubLevel = "scenes/dev_fmtv2.hxscene"

local BackgroundAmbience = GameObject.new("Sound")
BackgroundAmbience.SoundFile = "sounds/pixabay_soft-wind-314945.mp3"
BackgroundAmbience.Playing = true
BackgroundAmbience.Looped = true
BackgroundAmbience.Name = "BackgroundAmbience"
BackgroundAmbience.Parent = workspace

local animtest = scene_load("scenes/animtest.hxscene")[1]
animtest.Parent = workspace

local function loadSubLevel(Path)
	if not Path then
		print("No level was selected")
		return
	end
	
	for _, v in workspace.Level:GetChildren() do
		v:Destroy()
	end
	
	for _, root in scene_load(Path) do
		root.Parent = workspace.Level
	end
end

local totalelapsed = 0

local function update(DeltaTime)
	if imgui_begin("Model Importer") then
		ModelImporterScale = imgui_input_float("Scale", ModelImporterScale)

		if imgui_button("Select & Import Model") then
			imgui_end()
			
			local Path = fd_open("resources/models", "Models", "gltf;glb")[1]
			if Path then
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
			end
			
			return
		end
	end
	imgui_end()
	
	if imgui_begin("Run Script") then
		if imgui_textlink("Path: " .. ScriptExecutionFile) then
			imgui_end()
			local Path = fd_open("resources/scripts", "Luau Scripts", "luau")[1]
			if Path then
				ScriptExecutionFile = Path
			end
			return
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
	
	imgui_begin("File")
	
	if imgui_button("Save World") then
		imgui_end()
		
		IntendedSaveWorldState = game:Duplicate()
		IntendedSaveWorldState.Parent = nil
		
		local Path = fd_save("resources/scenes", "Scenes", "world")
		
		if Path then
			scene_save({ IntendedSaveWorldState }, Path)
		else
			print("No path selected")
		end
		
		IntendedSaveWorldState:Destroy()
		
		return
	end
	
	local loadWorld = imgui_button("Load World")

	if loadWorld then
		imgui_end()
		
		local Path = fd_open("resources/scenes", "Worlds", "world")[1]
		
		if Path then
			local dm = scene_load(Path)[1]
			if not dm then
				error("No root object!")
			end
			
			for _, v in game:GetChildren() do
				v:Destroy()
			end
			
			game:MergeWith(dm)
		else
			print("No path selected")
		end
		
		return
	end
	
	if imgui_button("Save Sub-Level") then
		imgui_end()
		
		IntendedLevelState = workspace.Level:Duplicate()
		IntendedLevelState.Parent = nil
		
		local Path = fd_save("resources/scenes", "Scenes", "hxscene")
		
		if Path then
			local actuallySave = {}
			for _, g in IntendedLevelState:GetChildren() do
				if g.Serializes then
					table.insert(actuallySave, g)
				end
			end
			
			scene_save(actuallySave, Path)
		else
			print("No path selected")
		end
		
		IntendedLevelState:Destroy()
		
		return
	end
	
	if imgui_button("Load Sub-Level") then
		imgui_end()
		loadSubLevel(fd_open("resources/scenes", "Scenes", "hxscene")[1])
		return
	end
	
	imgui_end()
	
	Editor(DeltaTime)
	--NodeEditor()

	if input_keypressed('m') then
		local sweeping = GameObject.new("Script")
		sweeping.SourceFile = "scripts/minesweeper.luau"
		sweeping.Name = "MinesweeperMain"
		sweeping.Parent = workspace

		workspace.Level.DirectionalLight.Parent = workspace
		workspace.Level:Destroy()
		script.Enabled = false
	end
	
	totalelapsed += DeltaTime
	
	if totalelapsed > 3 then
		animtest.Top.Transform = Matrix.fromEulerAnglesXYZ(math.sin(totalelapsed), 0, math.sin(totalelapsed))
	end
end

loadSubLevel(DefaultSubLevel)

if not conf_get("Developer") then
	update = function()
		if input_keypressed('m') then
			local sweeping = GameObject.new("Script")
			sweeping.SourceFile = "scripts/minesweeper.luau"
			sweeping.Name = "MinesweeperMain"
			sweeping.Parent = workspace
			
			workspace.Level.DirectionalLight.Parent = workspace
			workspace.Level:Destroy()
			script.Enabled = false
		end
	end
end

game.OnFrameBegin:Connect(update)

