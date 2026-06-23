-- stages/ProjectEditing.luau, 29/05/2026
local ProjectEditing = {}

local DeveloperTools = game:GetService("DeveloperTools")
local ScriptEngine = game:GetService("ScriptEngine")
local AssetManager = game:GetService("AssetManager")
local Collections = game:GetService("Collections")
local Physics = game:GetService("Physics")
local History = game:GetService("History")
local Logging = game:GetService("Logging")
local Engine = game:GetService("Engine")

local EditorCameraTransform = require("@ed/EditorCameraTransform")
local Configuration = require("@ed/Configuration")
local ErrorMessage = require("@ed/ErrorMessage")
local Preferences = require("@ed/Preferences")
local CoreTools = require("@ed/CoreTools")
local CommonUI = require("@ed/CommonUI")
local Reimport = require("@ed/Reimport")
local Output = require("@ed/Output")
local State = require("@ed/State")
local Types = require("@ed/Types")

local ProjectClosedByDataModelConnection = nil
local EditorCam = workspace.SceneCamera

local VisualizationsChangedConnections: { [GameObject]: {
    read Moved: EventConnection?,
    read WorkspaceChanged: EventConnection?,
} } = {}
local NewCameraCreatedConnection: EventConnection? = nil

local function makeVisualizerForObject(Object: GameObject, Visualization: GameObject & EcTransform)
    -- TODO luau cast
    local vis = Visualization:Duplicate() :: GameObject & EcTransform
    vis.Parent = CoreTools.VisualizationsContainer

    local conns = {}
    VisualizationsChangedConnections[Object] = conns

    if Object:HasComponent("Transform") then
        local t = Object :: EcTransform
        vis.Transform = t.Transform

        conns.Moved = t.OnScriptMoved:Connect(function(_, New)
            vis.Transform = New
            vis.Enabled = Object:HasComponent("Transform")
        end)
    end

    conns.WorkspaceChanged = Object.OnWorkspaceChanged:Connect(function(_, Current)
        vis.Enabled = Current == State.Project.Workspace
    end)

    CoreTools.VisualizationTargets[vis] = Object
end

function ProjectEditing.Enter(PreviousStage: Types.Stage)
    assert(State.Project.DataModel)
    assert(State.Project.Workspace)

    DeveloperTools:SetExplorerRoot(State.Project.DataModel)
    DeveloperTools:SetExplorerSelections({})
    Physics.Simulating = false
    CoreTools.VisualizationsContainer.Enabled = true

    if PreviousStage == "SelectProject" then
        EditorCameraTransform.Load(EditorCam)

        for icon, fstype in fs.listdir("@etextures/editor-icons") do
            if fstype == "f" and icon:find(".png") then
                AssetManager:QueueLoadTexture(icon, false)
            end
        end

        for _, reimportable in Collections:GetTagged(Configuration.ReimportableModelTag) :: { GameObject & EcModel & EcTransform } do
            if not reimportable:HasComponent("Model") or not reimportable:HasComponent("Transform") then
                continue
            end

            Reimport.WatchModel(reimportable)
        end

        for path, watcher in Reimport.Watchers do
            assert(watcher)

            if Reimport.ModelModifiedTimes[path] and Reimport.ModelModifiedTimes[path] < fs.lastwritten(path) then
                print(`Re-importing { path }`)

                local success, importedOrError = pcall(function()
                    return AssetManager:ImportModel(path)
                end)

                if not success then
                    Logging:Write(`Re-import failed on Project open: { importedOrError }`, Enum.LogType.Error)
                    continue
                end

                assert(typeof(importedOrError) == "GameObject")

                local action = History:TryBeginAction("Auto re-import on Project open")

                for _, model in watcher.Models :: { GameObject & EcModel } do
                    -- preserve tags, name, parent, object ID, treelinks

                    for _, child in model:GetChildren() do
                        child.Parent = nil -- Allow undo
                    end

                    for _, newChild in importedOrError:GetChildren() do
                        newChild:Duplicate().Parent = model
                    end
                end

                if action then
                    History:FinishAction(action)
                end

                Reimport.ModelModifiedTimes[path] = fs.lastwritten(path)
            end
        end

        ProjectClosedByDataModelConnection = State.Project.DataModel.Closing:Connect(function()
            assert(ProjectClosedByDataModelConnection)

            ProjectClosedByDataModelConnection:Disconnect()
            ProjectClosedByDataModelConnection = nil
            State.ChangeStageTo("SelectProject")
        end)

        for _, name in Preferences.DevToolsOpen do
            DeveloperTools:SetToolEnabled(name, true)
        end
    end

    for _, g in Collections:GetObjectsWithComponent("Camera", State.Project.DataModel) do
        -- TODO luau inferring `g` as `*error-type*`
        -- TODO luau cast https://github.com/luau-lang/luau/issues/2456
        --local cam: GameObject & EcTransform = CoreTools.CameraModel:Duplicate() :: any
        makeVisualizerForObject(g, CoreTools.CameraModel)
    end

    NewCameraCreatedConnection = Collections:GetComponentCreatedSignal("Camera", State.Project.DataModel):Connect(function(Camera)
        task.wait()
        print(Camera:GetFullName())
        makeVisualizerForObject(Camera, CoreTools.CameraModel)
    end)

    return nil
end

local ShowedAutosaveDelayedMessage = false

function ProjectEditing.Render(DeltaTime: number)
    assert(typeof(DeltaTime) == "number")

    CommonUI.RenderMainMenuBar(DeltaTime)
    CommonUI.RenderViewport(DeltaTime)

    if (game.Time - State.LastAutosaved) / 60 > Preferences.AutosaveInterval then
        if not History.CurrentActionName then
            State:SaveProject()
            print("Autosaved")
            State.LastAutosaved = game.Time
            ShowedAutosaveDelayedMessage = false

        elseif not ShowedAutosaveDelayedMessage then
            ErrorMessage.Push(`Autosave has been delayed until the { History.CurrentActionName } action finishes`)
            ShowedAutosaveDelayedMessage = true
        end
    end

    for vis, _ in CoreTools.VisualizationTargets do
        if vis == CoreTools.HoveringOverCamera then
            continue
        end

        local transparency = math.clamp(0.5 / vector.magnitude(vis.Transform.Position - workspace.SceneCamera.Transform.Position)^4, 0, 1)

        for _, g in vis:GetChildren() do
            if g:HasComponent("Mesh") then
                local m = g :: EcMesh
                m.Transparency = transparency
            end
        end
    end
end

function ProjectEditing.Leave(NextStage: Types.Stage)
    EditorCameraTransform.Save(EditorCam)
    CoreTools.VisualizationsContainer.Enabled = false

    if NextStage == "ProjectPlaying" then
        if Preferences.ClearOutputOnPlaytest then
            table.clear(Output.Log)
        end

        return
    else
        for _, watcher in Reimport.Watchers do
            assert(watcher)
            watcher.Disconnect()
        end

        table.clear(Reimport.Watchers)
    end

    for o, cs in VisualizationsChangedConnections do
        if cs.Moved then
            cs.Moved:Disconnect()
        end
        if cs.WorkspaceChanged then
            cs.WorkspaceChanged:Disconnect()
        end

        o:Destroy()
    end

    table.clear(VisualizationsChangedConnections)

    if NewCameraCreatedConnection then
        NewCameraCreatedConnection:Disconnect()
    end

    DeveloperTools:SetExplorerRoot(game)
    DeveloperTools:SetExplorerSelections({})

    assert(State.Project.DataModel)
    if not Preferences.ClosingProjectSaveRequiresConfirmation
        or Engine:ShowMessageBox("Save?", "Save datamodel before closing the current project?", "yesno", "question", 1) == 1
    then
        State:SaveProject()
    end

    State.Project.DataModel:Destroy()
    State.Project.Workspace = nil
    State.Project.Name = "Should not be visible"
    State.Project.IsTempPath = true
    State.Project.DataModel = nil

    Engine:SaveConfig()
    ScriptEngine:CloseVM(Configuration.GameEditingVMName)
end

return ProjectEditing
