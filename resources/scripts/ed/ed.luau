-- ed.luau, 14/09/2025
local Collections = game:GetService("Collections")
local Renderer = game:GetService("Renderer")
local History = game:GetService("History")
local Network = game:GetService("Network")
local Logging = game:GetService("Logging")
local Engine = game:GetService("Engine")

fs.definealias("editres", "./resources")
fs.definealias("etextures", "@editres/textures")
fs.definealias("base", "./resources")

local PluginManager = require("@ed/PluginManager")
local Preferences = require("@ed/Preferences")
local State = require("@ed/State")

--EditorPreferences = EditorPreferences :: { [keyof<typeof(EditorPreferences)>]: index<typeof(EditorPreferences), keyof<typeof(EditorPreferences)>> } & typeof(EditorPreferences)

History:EnableRecording()

local savedEditorPreferences = if fs.isfile("./editor/settings.json") then json.parse(assert(fs.read("./editor/settings.json"))) else EditorPreferences
assert(typeof(savedEditorPreferences) == "table")
savedEditorPreferences = savedEditorPreferences :: { [string | number]: JsonSerializable }

for k: keyof<typeof(EditorPreferences)>, v in EditorPreferences do
    if savedEditorPreferences[k] == nil then
        savedEditorPreferences[k] = v
    end
end

EditorPreferences = savedEditorPreferences :: typeof(EditorPreferences)

if not Engine.IsHeadless then
    imgui.stylecolors(if EditorPreferences.UseLightTheme then 'l' else 'd')
end

if not fs.isdirectory("./editor") then
    fs.mkdir("./editor")
    fs.mkdir("./editor/themes")
end

if not fs.isdirectory(EditorPreferences.ProjectsDirectory) then
    fs.mkdir(EditorPreferences.ProjectsDirectory)
end

local GitIsAvailable = fs.execute("git --version"):find("git version") ~= nil
print(if GitIsAvailable then "Git is available" else "Git is not available")

local updateInProgress = false
local updaterGitHubToken = ""
local updaterLog = ""
local updaterErr = nil
local switchedToUpdater = 0

local function runUpdate()
    updaterLog = "Update has started.\n"
    updaterErr = nil

    local headers = {
        ["Authorization"] = `Bearer { updaterGitHubToken }`,
        ["User-Agent"] = "PhoenixEngine/EditorUpdate"
    }

    updaterLog ..= "Finding latest workflow run...\n"
    local workflowRunResponse = Network:MakeHttpRequestAsync({
        Url = "https://api.github.com/repos/PhoenixWhitefire/PhoenixEngine/actions/workflows/cmake-multi-platform.yml/runs",
        Headers = headers
    })

    if not workflowRunResponse.Ok then
        error(`{ workflowRunResponse.Status }: { workflowRunResponse.Body }`)
    end

    local workflowRunResponseData = json.parse(workflowRunResponse.Body)
    assert(typeof(workflowRunResponseData) == "table", "Workflow run response data was not a table")
    workflowRunResponseData = workflowRunResponseData :: { [string]: JsonSerializable }

    updaterLog ..= `Total count: { assert(workflowRunResponseData.total_count) }\n`
    updaterLog ..= `Received: { #assert(workflowRunResponseData.workflow_runs :: { JsonSerializable }) }\n`

    local latest = (workflowRunResponseData.workflow_runs :: { { [string]: JsonSerializable } })[1]
    assert(latest)

    updaterLog ..= `Latest title: { assert(latest.display_title) }\n`

    local artifactsUrl = assert(latest.artifacts_url)
    assert(typeof(artifactsUrl) == "string")
    updaterLog ..= `Got artifacts URL: { artifactsUrl }\n`

    local jobsUrl = assert(latest.jobs_url)
    assert(typeof(jobsUrl) == "string")
    updaterLog ..= `Got jobs URL { jobsUrl }\n`

    local jobsResponse = Network:MakeHttpRequestAsync({
        Url = jobsUrl,
        Headers = headers
    })

    if not jobsResponse.Ok then
        error(`{ jobsResponse.Status }: { jobsResponse.Body }`)
    end

    local jobsJson = json.parse(jobsResponse.Body)
    assert(typeof(jobsJson) == "table")

    local jobs = (jobsJson :: { [string]: JsonSerializable }).jobs
    assert(jobs and typeof(jobs) == "table")

    local desiredName = `{ if Engine.TargetPlatform == "Linux" then "ubuntu" else Engine.TargetPlatform:lower() }`
    local foundJob = false

    for _, job in jobs do
        assert(typeof(job) == "table")

        local name =  (job :: { [string]: JsonSerializable }).name
        assert(typeof(name) == "string")

        if name:find(desiredName) and name:find("Release") then
            if (job :: { [string]: JsonSerializable }).conclusion ~= "success" then
                error("The Workflow Job for your platform failed or is still in progress.")
            end

            foundJob = true
            break
        end
    end

    if not foundJob then
        error(`Could not find the Job for your platform ({ desiredName })`)
    end

    updaterLog ..= "Confirmed that the Job for your platform succeeded, proceeding to download artifact\n"

    local artifactsResponse = Network:MakeHttpRequestAsync({
        Url = artifactsUrl,
        Headers = headers
    })

    if not artifactsResponse.Ok then
        error(`{ artifactsResponse.Status }: { artifactsResponse.Body }`)
    end

    local artifactsData = json.parse(artifactsResponse.Body)
    assert(typeof(artifactsData) == "table")
    artifactsData = artifactsData :: { [string]: JsonSerializable }

    updaterLog ..= `Artifact count: { assert(artifactsData.total_count) }\n`
    updaterLog ..= `Checking for { Engine.TargetPlatform } Release artifact...\n`

    assert(typeof(artifactsData.artifacts) == "table")
    local releaseArtifact

    for _, v in artifactsData.artifacts do
        assert(typeof(v) == "table")
        v = v :: { [string]: JsonSerializable }

        local name = v.name
        assert(typeof(name) == "string")

        if name:find(Engine.TargetPlatform) and name:find("Release") then
            releaseArtifact = v
            break
        end
    end

    if not releaseArtifact then
        error(`Could not find { Engine.TargetPlatform }-Release artifact`)
    end

    updaterLog ..= "Found Release artifact\n"
    local bytes = assert(releaseArtifact.size_in_bytes)
    assert(typeof(bytes) == "number")

    updaterLog ..= string.format("Downloading %.2fMB (%i bytes)...\n", bytes / 1000 / 1000, bytes)
    local downloadUrl = assert(releaseArtifact.archive_download_url)
    assert(typeof(downloadUrl) == "string")

    local zipResponse = Network:MakeHttpRequestAsync({
        Url = downloadUrl,
        Headers = headers
    })

    if not zipResponse.Ok then
        error(`{ zipResponse.Status }: { zipResponse.Body }`)
    end

    updaterLog ..= `Got { #zipResponse.Body } bytes\n`
    updaterLog ..= "Writing to update.zip...\n"

    if fs.isfile("./update.zip") then
        error("`update.zip` already exists in the installation directory!")
    end

    fs.write("./update.zip", zipResponse.Body)
    updaterLog ..= "All done!\n"

    task.wait() -- allow the `updaterLog` to be rendered
    Engine:ShowMessageBox("Update", "Close the Editor, extract the 'update.zip' file into the Editor's directory, and you're all done!")
end

function Stages.Update(dt: number)
    Stages.SelectProject(dt)

    if game.Time - switchedToUpdater < 0.1 then
        imgui.setnextwindowsize(750, 500)
        imgui.setnextwindowposition(Renderer.Resolution.x / 2 - 750 / 2, Renderer.Resolution.y / 2 - 500 / 2)
    end

    if imgui.beginpopupmodal("Updater", true) then
        imgui.text("The Updater will download the latest GitHub Actions artifact, which requires a GitHub token.")
        imgui.urllink("Generate a Personal Access Token", "https://github.com/settings/personal-access-tokens")

        imgui.text("GitHub token: ")
        imgui.sameline()

        imgui.setcursorposition(imgui.cursorposition() + vector.create(140, 0))
        if not updateInProgress then
            updaterGitHubToken = imgui.inputstring("##Token", updaterGitHubToken, true)
        else
            imgui.text(string.rep("*", #updaterGitHubToken))
        end

        if not updateInProgress then
            local cxavail = imgui.contentregionavail().x
            local cursor = imgui.cursorposition()
            local btsx = imgui.textsize("Begin").x

            imgui.setcursorposition(getCenteredX(cursor.x, cxavail, btsx), cursor.y)
            if imgui.button("Begin") then
                task.defer(function()
                    local s, e = (pcall :: any)(function()
                        runUpdate()
                    end)

                    if not s then
                        updaterLog ..= "Update failed:\n"
                        updaterErr = e
                    end
                end)

                updateInProgress = true
            end
        end

        if #updaterLog > 0 then
            imgui.separator()
            imgui.text(updaterLog)

            if updaterErr then
                imgui.pushstylecolor(0, { 1, 0, 0, 1 })
                imgui.text(updaterErr)
                imgui.popstylecolor()
            end
        end

        imgui.endpopup()

    elseif game.Time - switchedToUpdater > 0.1 and (not updateInProgress or updaterErr) then
        changeStageTo("SelectProject" :: "SelectProject")

    else
        if game.Time - switchedToUpdater > 0.1 then
            setErrorNotification("You can't do that")
        end

        imgui.openpopup("Updater")
    end
end

function Stages.Update_Leave()
    imgui.openpopup("About")
    canUpdate = true
    updaterErr = nil
    updateInProgress = false
end

local function handleEngineShutdown()
    saveSettings()

    if CurrentStage == "ProjectEditing" or CurrentStage == "ProjectPlaying" then
        local choice = Engine:ShowMessageBox("Save", "Save current Editor file before closing?", "yesno", "warning", 1)

        if choice == 0 then
            warn("Quitting without saving")
        else
            print("Saving before quitting")
            saveProject()
        end

        if Project.DataModel then
            Project.DataModel:Destroy()
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
    if _state.Project.DataModel then
        errorMessage ..= "\n\nThe Editor will perform a save to the file 'resources/scenes/autosave.world', please ensure nothing important is stored at that location before proceeding."
    end

    Logging:Write(errorMessage, Enum.LogType.Error)
    Engine:ShowMessageBox("Editor Error", errorMessage, "ok", "error")

    if _state.Project.DataModel then
        local s, e = AssetManager:SaveScene({ _state.Project.DataModel }, "scenes/autosave.world")
        assert(s, `Failed to save: {e}`)
    end
end

local function editorFrame(State: StateType, DeltaTime)
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
    Stages[CurrentStage](DeltaTime)
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

local function changeStageTo(NewStage: string)
    assert(Stages[NewStage])

    if StageUpdateConnection then
        StageUpdateConnection:Disconnect()
        StageUpdateConnection = nil
    end

    if Stages[CurrentStage .. "_Leave"] then
        Stages[CurrentStage .. "_Leave"](NewStage)
    end

    local targetDm = game
    if Stages[NewStage .. "_Enter"] then
        targetDm = Stages[NewStage .. "_Enter"](CurrentStage) or targetDm
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

        local success = xpcall(createNewProject_, function(err)
            warn(debug.traceback(err, 1))
        end)

        if not success then
            warn("Failed to create new project!")
            game:Close(1)
            return
        end

        if CurrentStage ~= "ProjectEditing" then
            warn(`Stage is {CurrentStage} and not ProjectEditing`)
            game:Close(1)
            return
        end

        warn("Editor CI checks done")
        CurrentStage = "SelectProject"
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
        openProject(name)
    end
end
