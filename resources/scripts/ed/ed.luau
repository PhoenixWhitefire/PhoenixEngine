-- ed.luau, 14/09/2025
local AssetManager = game:GetService("AssetManager")
local PlayerInput = game:GetService("PlayerInput")
local History = game:GetService("History")
local Logging = game:GetService("Logging")
local Engine = game:GetService("Engine")

fs.definealias("editres", "./resources")
fs.definealias("etextures", "@editres/textures")
fs.definealias("base", "./resources")

local PluginManager = require("@ed/PluginManager")
local ErrorMessage = require("@ed/ErrorMessage")
local Preferences = require("@ed/Preferences")
local CoreTools = require("@ed/CoreTools")
local CommonUI = require("@ed/CommonUI")
local State = require("@ed/State")
local Types = require("@ed/Types")

local Stages = require("@ed/Stages")

Preferences = Preferences :: { [keyof<typeof(Preferences)>]: index<typeof(Preferences), keyof<typeof(Preferences)>> } & typeof(Preferences)

History:EnableRecording()

local savedEditorPreferences = if fs.isfile("./editor/settings.json") then json.parse(assert(fs.read("./editor/settings.json"))) else Preferences
assert(typeof(savedEditorPreferences) == "table")
savedEditorPreferences = savedEditorPreferences :: { [string | number]: JsonSerializable }

for k: keyof<typeof(Preferences)>, v in Preferences do
    if savedEditorPreferences[k] == nil then
        savedEditorPreferences[k] = v
    end
end

Preferences = savedEditorPreferences :: typeof(Preferences)

if not Engine.IsHeadless then
    imgui.stylecolors(if Preferences.UseLightTheme then 'l' else 'd')
end

if not fs.isdirectory("./editor") then
    fs.mkdir("./editor")
    fs.mkdir("./editor/themes")
end

if not fs.isdirectory(Preferences.ProjectsDirectory) then
    fs.mkdir(Preferences.ProjectsDirectory)
end

local function handleEngineShutdown()
    CommonUI.SaveSettings()

    if State.CurrentStage == "ProjectEditing" or State.CurrentStage == "ProjectPlaying" then
        local choice = Engine:ShowMessageBox("Save", "Save current Editor file before closing?", "yesno", "warning", 1)

        if choice == 0 then
            warn("Quitting without saving")
        else
            print("Saving before quitting")
            State:SaveProject()
        end

        if State.Project.DataModel then
            State.Project.DataModel:Destroy()
        end
    end
end

local function checkIfLatestVersionBuilt()
    local commitHashFetchResult = fs.execute("git rev-parse HEAD")

    -- commit hashes are 40 characters
    if #commitHashFetchResult < 40 then
        return
    end

    if #commitHashFetchResult == 41 and commitHashFetchResult:sub(41, 41) == "\n" then
        -- ends with newline
        commitHashFetchResult = commitHashFetchResult:sub(1, 40)
    end

    if #commitHashFetchResult == 40 and commitHashFetchResult ~= Engine.CommitHash then
        Engine:ShowMessageBox(
            "Version mismatch",
            `Your source files are on commit { commitHashFetchResult:sub(1, 8) }, but the Engine is running on commit { Engine.CommitHash:sub(1, 8) }.\n`
                .. "You may need to re-build the Engine. Behaviour of the Updater may be incorrect."
        )
    end
end

local function onError(err)
    assert(State)

    local errorMessage = `An error occurred: { debug.traceback(err, 2) }`
    if State.Project.DataModel then
        errorMessage ..= "\n\nThe Editor will perform a save to the file 'resources/scenes/autosave.world', please ensure nothing important is stored at that location before proceeding."
    end

    Logging:Write(errorMessage, Enum.LogType.Error)
    Engine:ShowMessageBox("Editor Error", errorMessage, "ok", "error")

    if State.Project.DataModel then
        local s, e = AssetManager:SaveScene({ State.Project.DataModel }, "scenes/autosave.world")
        assert(s, `Failed to save: {e}`)
    end
end

local WasHoldingCtrlKeyDown = false
local WasHoldingZKeyDown = false
local WasHoldingYKeyDown = false

local function handleUndoRedoKeybinds()
    local ctrlKeyDown = not imgui.anyitemactive() and PlayerInput:IsKeyPressed(Enum.Key.LeftControl)
    local zKeyDown    = not imgui.anyitemactive() and PlayerInput:IsKeyPressed(Enum.Key.Z)
    local yKeyDown    = not imgui.anyitemactive() and PlayerInput:IsKeyPressed(Enum.Key.Y)

    if ctrlKeyDown and zKeyDown then
        if not WasHoldingCtrlKeyDown or not WasHoldingZKeyDown then
            local success, error = pcall(function()
                History:Undo()
            end)

            if not success then
                assert(error)

                local realError = if typeof(error) == "string" then error:sub(error:find(":") or 1, #error) else tostring(error)
                realError = realError:sub((realError:find(" ") or 0) + 1, #realError)

                ErrorMessage.Push("Undo failed: " .. realError)
            end
        end
    end

    if ctrlKeyDown and yKeyDown then
        if not WasHoldingCtrlKeyDown or not WasHoldingYKeyDown then
            local success, error = pcall(function()
                History:Redo()
            end)

            if not success then
                assert(error)

                local realError = if typeof(error) == "string" then error:sub(error:find(":") or 1, #error) else tostring(error)
                realError = realError:sub((realError:find(" ") or 0) + 1, #realError)

                ErrorMessage.Push("Redo failed: " .. realError)
            end
        end
    end

    WasHoldingCtrlKeyDown = ctrlKeyDown
    WasHoldingZKeyDown = zKeyDown
    WasHoldingYKeyDown = yKeyDown
end

local function editorFrame(State, DeltaTime)
    if not workspace.SceneCamera then
        workspace.SceneCamera = GameObject.new("Camera")
        workspace.SceneCamera.Parent = workspace
    end

    if not History.IsRecordingAction then
        if State.CurrentStage == "ProjectEditing" then
            handleUndoRedoKeybinds()
        end
    end

    CoreTools.Update(State.Project, Preferences, State.ViewportRenderPosition, State.ViewportRenderSize, DeltaTime)

    -- TODO luau
    -- Stages[State.CurrentStage].Render(DeltaTime)

    if State.CurrentStage == "SelectProject" then
        Stages.SelectProject.Render(DeltaTime)
    elseif State.CurrentStage == "ProjectEditing" then
        Stages.ProjectEditing.Render(DeltaTime)
    elseif State.CurrentStage == "ProjectPlaying" then
        Stages.ProjectPlaying.Render(DeltaTime)
    else
        error(`Unexpected editor stage '{ State.CurrentStage }'`)
    end
end

local function stageUpdate(State, DeltaTime)
    local s, e = (xpcall::any)(function(State, dt)
        editorFrame(State, dt)
    end, onError, State, DeltaTime)

    if not s then
        warn(`editorFrame error: {e}`)
    end
end

local StageUpdateConnection: EventConnection?

local function changeStageTo(NewStage: Types.Stage)
    assert(NewStage ~= "None" and NewStage ~= "Update")
    assert(State.CurrentStage ~= "None" and State.CurrentStage ~= "Update")
    assert(Stages[NewStage])

    if StageUpdateConnection then
        StageUpdateConnection:Disconnect()
        StageUpdateConnection = nil
    end

    if Stages[State.CurrentStage].Leave then
        Stages[State.CurrentStage].Leave(NewStage)
    end

    local targetDm = game
    if Stages[NewStage].Enter then
        targetDm = Stages[NewStage].Enter(State.CurrentStage) or targetDm
    end

    PluginManager.OnEditorStageChanged(State.CurrentStage, NewStage)
    CoreTools.OnEditorStageChanged(NewStage)
    StageUpdateConnection = targetDm.OnFrameBegin:Connect(function(dt)
        stageUpdate(State, dt)
    end)

    State.CurrentStage = NewStage
end

game:BindToClose(handleEngineShutdown)
task.defer(checkIfLatestVersionBuilt)

if table.find(Engine:GetCliArguments(), "-ed_ci") then
    -- TODO luau cast
    task.defer(changeStageTo, "SelectProject" :: "SelectProject")

    if StageUpdateConnection then
        StageUpdateConnection:Disconnect()
        StageUpdateConnection = nil
    end

    coroutine.wrap(function()
        warn("Running Editor CI checks")

        task.delay(5, function()
            warn("Potential CI stall - Exiting")
            game:Close(1)
        end)

        local success = xpcall(State.CreateNewProject, function(err)
            warn(debug.traceback(err, 1))
        end, State, false)

        if not success then
            warn("Failed to create new project!")
            game:Close(1)
            return
        end

        if State.CurrentStage ~= "ProjectEditing" then
            warn(`Stage is {State.CurrentStage} and not ProjectEditing`)
            game:Close(1)
            return
        end

        warn("Editor CI checks done")
        State.CurrentStage = "SelectProject"
        game:Close()
    end)()
else
    changeStageTo("SelectProject")
end

local loadProjectIndex = table.find(Engine:GetCliArguments(), "--project")
if loadProjectIndex then
    local name = Engine:GetCliArguments()[loadProjectIndex + 1]
    if not name then
        Logging:Write("`--project` specified without the project name!", Enum.LogType.Error)
    else
        State:OpenProject(name)
    end
end
