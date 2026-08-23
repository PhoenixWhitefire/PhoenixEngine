-- stages/ProjectEditing.luau, 29/05/2026
local ProjectEditing = {}

local DeveloperTools = game:GetService("DeveloperTools")
local ScriptEngine = game:GetService("ScriptEngine")
local AssetManager = game:GetService("AssetManager")
local Collections = game:GetService("Collections")
local Workspace = game:GetService("Workspace")
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
local EditorCam = Workspace.SceneCamera

local VisualizationsChangedConnections: { [GameObject]: {
    read Moved: EventConnection?,
    read WorkspaceChanged: EventConnection?,
} } = {}
local NewCameraCreatedConnection: EventConnection? = nil
-- TODO luau removing annotation shows leftover free type
local Breakpoints: { [string]: { [number]: DebugBreakpointInfo } } = {}
ProjectEditing.Breakpoints = Breakpoints

type BreakpointsData = { File: string, Breakpoints: { DebugBreakpointInfo }? }

local function valuesOf<I, V>(t: { read [I]: V }): { V }
    local values = {}

    for _, v in t do
        table.insert(values, v)
    end

    return values
end

DeveloperTools.BreakpointUpdated:Connect(function(file, breakpoint)
    local breakpointsForScript = Breakpoints[file]

    if not breakpointsForScript then
        breakpointsForScript = {}
        Breakpoints[file] = breakpointsForScript
    end

    breakpointsForScript[breakpoint.Line] = breakpoint

    if State.CurrentStage == "ProjectPlaying" then
        local realLine = ScriptEngine:SetScriptBreakpoint(Configuration.PlaytestVMName, file, breakpoint.Line, breakpoint.Enabled or breakpoint.ConditionEnabled)

        if realLine == -1 then
            warn(`Failed to apply breakpoint { file }:{ breakpoint.Line }`);
        elseif breakpoint.Line ~= realLine then
            breakpointsForScript[breakpoint.Line] = nil
            breakpointsForScript[realLine] = breakpoint
            breakpoint.Line = realLine;
        end
    end

    DeveloperTools:SetDocumentBreakpoints(file, valuesOf(breakpointsForScript))
end)

DeveloperTools.BreakpointRemoved:Connect(function(file, line)
    local breakpointsForScript = Breakpoints[file]

    if breakpointsForScript then
        if breakpointsForScript[line] then
            breakpointsForScript[line] = nil
        end
    else
        breakpointsForScript = {}
        Breakpoints[file] = {}
    end

    if State.CurrentStage == "ProjectPlaying" then
        local realLine = ScriptEngine:SetScriptBreakpoint(Configuration.PlaytestVMName, file, line, false)

        if realLine == -1 then
            warn(`Breakpoint { file }:{ line } not present at removal`)
        else
            if breakpointsForScript[realLine] then
                breakpointsForScript[realLine] = nil
            end
        end
    end

    DeveloperTools:SetDocumentBreakpoints(file, valuesOf(breakpointsForScript or {}))
end)

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
    History.RecordingEnabled = true

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

        local edFile = json.parse(fs.read("@project/phoenix.editor") :: string) :: { [string]: JsonEncodable }

        if edFile.OpenDocuments then
            for _, doc in edFile.OpenDocuments :: { string } do
                DeveloperTools:OpenTextDocument(doc, 1, false)
            end
        end

        if edFile.Breakpoints then
            for _, breakpointsInfo in edFile.Breakpoints :: { BreakpointsData } do
                DeveloperTools:SetDocumentBreakpoints(breakpointsInfo.File, assert(breakpointsInfo.Breakpoints))

                local dictionary = {}
                for _, bp in breakpointsInfo.Breakpoints do
                    dictionary[bp.Line] = bp
                end

                Breakpoints[breakpointsInfo.File] = dictionary
            end
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

        local transparency = math.clamp(0.5 / vector.magnitude(vis.Transform.Translation - Workspace.SceneCamera.Transform.Translation)^4, 0, 1)

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
    History.RecordingEnabled = false

    if NextStage == "ProjectPlaying" then
        LastSelectedInEditMode = DeveloperTools:GetExplorerSelections()
        DeveloperTools:SaveTextDocuments()

        if Preferences.ClearOutputOnPlaytest then
            table.clear(Output.Log)
        end

        return
    end

    local openDocuments = DeveloperTools:GetOpenTextDocuments()

    local edFile = json.parse(fs.read("@project/phoenix.editor") :: string) :: { [string]: JsonEncodable }
    edFile.OpenDocuments = openDocuments

    local breakpointInfos = edFile.Breakpoints :: { BreakpointsData }?
    if not breakpointInfos then
        breakpointInfos = {} :: { BreakpointsData }
        -- TODO luau cast
        edFile.Breakpoints = breakpointInfos :: { [string]: JsonEncodable }
    end
    assert(breakpointInfos)

    for _, doc in openDocuments do
        local info: BreakpointsData = {
            File = doc,
            Breakpoints = DeveloperTools:GetDocumentBreakpoints(doc)
        }

        local hasExisting = false

        for index, existing in breakpointInfos :: { BreakpointsData } do
            if existing.File == doc then
                breakpointInfos[index] = info
                hasExisting = true

                break
            end
        end

        if not hasExisting then
            table.insert(breakpointInfos, info)
        end
    end

    fs.write("@project/phoenix.editor", json.encode(edFile))
    fs.remove("@project/phoenix.lock")

    DeveloperTools:CloseTextDocuments()

    for _, watcher in Reimport.Watchers do
        assert(watcher)
        watcher.Disconnect()
    end

    table.clear(Reimport.Watchers)

    for o, cs in VisualizationsChangedConnections do
        -- TODO .Connected checks?
        if cs.Moved and cs.Moved.Connected then
            cs.Moved:Disconnect()
        end
        if cs.WorkspaceChanged and cs.WorkspaceChanged.Connected then
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

    Engine:UnbindDataModel(assert(State.Project.DataModel))
    ScriptEngine:CloseVM(Configuration.GameEditingVMName)

    State.Project.DataModel:Destroy()
    State.Project.Workspace = nil
    State.Project.Name = "Should not be visible"
    State.Project.IsTempPath = true
    State.Project.DataModel = nil
end

return ProjectEditing
