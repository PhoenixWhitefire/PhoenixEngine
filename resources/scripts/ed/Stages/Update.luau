-- stages/Update.luau, 29/05/2026
local Update = {}

local Renderer = game:GetService("Renderer")
local Network = game:GetService("Network")
local Engine = game:GetService("Engine")

local ErrorMessage = require("@ed/ErrorMessage")
local CommonUI = require("@ed/CommonUI")
local State = require("@ed/State")

function Update.Enter()
    imgui.openpopup("Updater")
    canUpdate = false
    switchedToUpdater = game.Time
end

function Update.Leave()
    imgui.openpopup("About")
    canUpdate = true
    updaterErr = nil
    updateInProgress = false
end

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
    workflowRunResponseData = workflowRunResponseData

    updaterLog ..= `Total count: { assert(workflowRunResponseData.total_count) }\n`
    updaterLog ..= `Received: { #assert(workflowRunResponseData.workflow_runs :: { JsonDecodable }) }\n`

    local latest = (workflowRunResponseData.workflow_runs :: { { [string]: JsonDecodable } })[1]
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

    local jobs = jobsJson.jobs
    assert(jobs and typeof(jobs) == "table")

    local desiredName = `{ if Engine.TargetPlatform == "Linux" then "ubuntu" else Engine.TargetPlatform:lower() }`
    local foundJob = false

    for _, job in jobs do
        assert(typeof(job) == "table")

        local name =  job.name
        assert(typeof(name) == "string")

        if name:find(desiredName) and name:find("Release") then
            if job.conclusion ~= "success" then
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
    artifactsData = artifactsData

    updaterLog ..= `Artifact count: { assert(artifactsData.total_count) }\n`
    updaterLog ..= `Checking for { Engine.TargetPlatform } Release artifact...\n`

    assert(typeof(artifactsData.artifacts) == "table")
    local releaseArtifact

    for _, v in artifactsData.artifacts do
        assert(typeof(v) == "table")

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

function Update.Render(dt: number)
    --Stages.SelectProject(dt)

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

            imgui.setcursorposition(CommonUI.GetCenteredX(cursor.x, cxavail, btsx), cursor.y)
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
                imgui.pushstylecolor(Enum.ImGuiColor.Text, { 1, 0, 0, 1 })
                imgui.text(updaterErr)
                imgui.popstylecolor()
            end
        end

        imgui.endpopup()

    elseif game.Time - switchedToUpdater > 0.1 and (not updateInProgress or updaterErr) then
        State.ChangeStageTo("SelectProject")

    else
        if game.Time - switchedToUpdater > 0.1 then
            ErrorMessage.Push("You can't do that")
        end

        imgui.openpopup("Updater")
    end
end

return Update
