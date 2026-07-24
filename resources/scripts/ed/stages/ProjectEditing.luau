-- stages/ProjectEditing.luau, 29/05/2026
local ProjectEditing = {}

local DeveloperTools = game:GetService("DeveloperTools")
local ScriptEngine = game:GetService("ScriptEngine")
local AssetManager = game:GetService("AssetManager")
local Collections = game:GetService("Collections")
local Physics = game:GetService("Physics")
local History = game:GetService("History")
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
local LastSelectedInEditMode = {}
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

        for icon, fstype in fs.listdir("@_etextures/editor-icons") do
            if fstype == "f" and icon:find(".png") then
                AssetManager:QueueLoadTexture(icon, false)
            end
        end

        Reimport.CheckAll()

        ProjectClosedByDataModelConnection = State.Project.DataModel.Closing:Connect(function()
            assert(ProjectClosedByDataModelConnection)

            ProjectClosedByDataModelConnection:Disconnect()
            ProjectClosedByDataModelConnection = nil
            State.ChangeStageTo("SelectProject")
        end)

        for _, name in Preferences.DevToolsOpen do
            DeveloperTools:SetToolEnabled(name, true)
        end
    elseif PreviousStage == "ProjectPlaying" then
        DeveloperTools:SetExplorerSelections(LastSelectedInEditMode)
        table.clear(LastSelectedInEditMode)
    end

    for _, g in Collections:GetObjectsWithComponent("Camera", State.Project.DataModel) do
        -- TODO luau inferring `g` as `*error-type*`
        -- TODO luau cast https://github.com/luau-lang/luau/issues/2456
        --local cam: GameObject & EcTransform = CoreTools.CameraModel:Duplicate() :: any
        makeVisualizerForObject(g, CoreTools.CameraModel)
    end

    NewCameraCreatedConnection = Collections:GetComponentCreatedSignal("Camera", State.Project.DataModel):Connect(function(Camera)
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

        local transparency = math.clamp(0.5 / vector.magnitude(vis.Transform.Translation - workspace.SceneCamera.Transform.Translation)^4, 0, 1)

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
        LastSelectedInEditMode = DeveloperTools:GetExplorerSelections()
        DeveloperTools:SaveTextDocuments()

        if Preferences.ClearOutputOnPlaytest then
            table.clear(Output.Log)
        end

        return
    else
        DeveloperTools:CloseTextDocuments()

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
    end

    table.clear(VisualizationsChangedConnections)

    for _, g in CoreTools.VisualizationsContainer:GetChildren() do
        g:Destroy()
    end

    table.clear(CoreTools.VisualizationTargets)

    if NewCameraCreatedConnection then
        NewCameraCreatedConnection:Disconnect()
        NewCameraCreatedConnection = nil
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
