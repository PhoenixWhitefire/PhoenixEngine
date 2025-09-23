-- root.luau, 21/12/2024
--!strict

-- local NodeEditor = nil --require("scripts/modules/NodeEditor.luau")
local Editor = function() end--require("scripts/modules/Editor.luau")

local ModelImporterScale = 1

local ScriptExecutionFile = "scripts/mc.luau"

local DefaultSubLevel = "scenes/dev_fmtv2.hxscene"

local BackgroundAmbience = GameObject.new("Sound") :: (GameObject & Sound)
BackgroundAmbience.SoundFile = "sounds/pixabay_soft-wind-314945.mp3"
BackgroundAmbience.Playing = true
BackgroundAmbience.Looped = true
BackgroundAmbience.Name = "BackgroundAmbience"
BackgroundAmbience.Serializes = false
BackgroundAmbience.Parent = workspace

local ErrorMessage = "No error dummy"
local ErrorMessageTimeRemaining = 0

local CommandLine = ""
local LightTheme = false

local function setError(errm)
	ErrorMessage = errm
	ErrorMessageTimeRemaining = 2
	print(errm)
end

local function loadSubLevel(Path)
	if not Path then
		print("No level was selected")
		return
	end
	
	if workspace:FindChild("Level") then
		workspace.Level:Destroy()
	end
	
	local lvl = GameObject.new("Model")
	lvl.Name = "Level"
	lvl.Parent = workspace
	
	for _, root in assert(scene.load(Path)) do
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
		
		if imgui.button("Execute file") then
			local co, err = loadfile(ScriptExecutionFile)
			if not co then
				setError(err)
			else
				coroutine.resume(co)
			end
		end
		
		CommandLine = imgui.inputstring("Command", CommandLine or "")
		
		if imgui.button("Execute command") then
			local co, err = loadthread(CommandLine)
			if not co then
				setError(err)
			else
				coroutine.resume(co)
			end
		end
		
	end
	imgui.endw()
	
	imgui.begin("File")
	
	if imgui.button("Save World") then
		imgui.endw()
		
		local Path = fs.promptsave("resources/scenes", "world", "Worlds")
		
		if Path then
			scene.save({ game }, Path)
		else
			print("No path selected")
		end
		
		return
	end
	
	if imgui.button("Load World") then
		imgui.endw()
		
		local Path = fs.promptopen("resources/scenes", "world", "Worlds")[1]
		
		if Path then
			local dm = assert(scene.load(Path))[1]
			if not dm then
				error("No root object!")
			end
			
			for _, v: GameObject in game:GetChildren() do
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
		
		local Path = fs.promptsave("resources/scenes", "hxscene", "Scenes")
		
		if Path then
			local actuallySave = {}
			for _, g: GameObject in workspace.Level:GetChildren() do
				if g.Serializes then
					table.insert(actuallySave, g)
				end
			end
			
			print("Saving to", Path)
			scene.save(actuallySave, Path)
		else
			print("No path selected")
		end
		
		return
	end
	
	if imgui.button("Load Sub-Level") then
		imgui.endw()
		loadSubLevel(fs.promptopen("resources/scenes", "hxscene", "Scenes")[1])
		return
	end
	
	local prevLightTheme = LightTheme
	LightTheme = imgui.checkbox("Light-mode", LightTheme or false)
	if LightTheme ~= prevLightTheme then
		imgui.stylecolors(if LightTheme then "L" else "D")
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
end

if not workspace:FindChild("Level") then
	loadSubLevel(DefaultSubLevel)
end

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

