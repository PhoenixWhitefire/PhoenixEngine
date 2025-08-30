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

local animtest = ({ scene.load("scenes/animtest.hxscene") })[2][1]
animtest.Parent = workspace

local function loadSubLevel(Path)
	if not Path then
		print("No level was selected")
		return
	end
	
	for _, v in workspace.Level:GetChildren() do
		v:Destroy()
	end
	
	for _, root in ({scene.load(Path)})[2] do
		root.Parent = workspace.Level
	end
end

local function update(DeltaTime)
	if imgui.begin("Model Importer") then
		ModelImporterScale = imgui.inputnumber("Scale", ModelImporterScale)

		if imgui.button("Select & Import Model") then
			imgui.endw()
			
			local Path = fs.promptopen("models", "gltf;glb", "Models")[1]
			if Path then
				local pathSplit = string.split(Path, "/")
				
				local parts = model.import(Path)
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
	imgui.endw()
	
	if imgui.begin("Run Script") then
		if imgui.textlink("Path: " .. ScriptExecutionFile) then
			imgui.endw()
			local Path = fs.promptopen("resources/scripts", "luau", "Luau Scripts")[1]
			if Path then
				ScriptExecutionFile = Path
			end
			return
		end
		
		if imgui.button("Execute") then
			if scriptHost then
				scriptHost:Destroy()
			end
			scriptHost = GameObject.new("Script")
			scriptHost.Name = "TempScriptHost"
			scriptHost.SourceFile = ScriptExecutionFile
			scriptHost.Parent = script
		end
	end
	imgui.endw()
	
	imgui.begin("File")
	
	if imgui.button("Save World") then
		imgui.endw()
		
		IntendedSaveWorldState = game:Duplicate()
		IntendedSaveWorldState.Parent = nil
		
		local Path = fs.promptsave("resources/scenes", "world", "Worlds")
		
		if Path then
			scene.save({ IntendedSaveWorldState }, Path)
		else
			print("No path selected")
		end
		
		IntendedSaveWorldState:Destroy()
		
		return
	end
	
	local loadWorld = imgui.button("Load World")

	if loadWorld then
		imgui.endw()
		
		local Path = fs.promptopen("resources/scenes", "world", "Worlds")[1]
		
		if Path then
			local dm = scene.load(Path)[1]
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
	
	if imgui.button("Save Sub-Level") then
		imgui.endw()
		
		IntendedLevelState = workspace.Level:Duplicate()
		IntendedLevelState.Parent = nil
		
		local Path = fs.promptsave("resources/scenes", "hxscene", "Scenes")
		
		if Path then
			local actuallySave = {}
			for _, g in IntendedLevelState:GetChildren() do
				if g.Serializes then
					table.insert(actuallySave, g)
				end
			end
			
			scene.save(actuallySave, Path)
		else
			print("No path selected")
		end
		
		IntendedLevelState:Destroy()
		
		return
	end
	
	if imgui.button("Load Sub-Level") then
		imgui.endw()
		loadSubLevel(fs.promptopen("resources/scenes", "hxscene", "Scenes")[1])
		return
	end
	
	imgui.endw()
	
	Editor(DeltaTime)
	--NodeEditor()

	if input.keypressed('m') then
		local sweeping = GameObject.new("Script")
		sweeping.SourceFile = "scripts/minesweeper.luau"
		sweeping.Name = "MinesweeperMain"
		sweeping.Parent = workspace

		workspace.Level.DirectionalLight.Parent = workspace
		workspace.Level:Destroy()
		script.Enabled = false
	end
	
	local top = animtest:FindChild("Top")
	if top then
		top.Transform = Matrix.fromEulerAnglesXYZ(math.sin(game.Time), 0, math.sin(game.Time))
	end
end

loadSubLevel(DefaultSubLevel)

if not conf.get("Developer") then
	update = function()
		if input.keypressed('m') then
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

