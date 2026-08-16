-- stages/ProjectPlaying.luau, 29/05/2026
local ProjectPlaying = {}

local DeveloperTools = game:GetService("DeveloperTools")
local ScriptEngine = game:GetService("ScriptEngine")
local PlayerInput = game:GetService("PlayerInput")
local Physics = game:GetService("Physics")
local Engine = game:GetService("Engine")

local Configuration = require("@ed/Configuration")
local Preferences = require("@ed/Preferences")
local CommonUI = require("@ed/CommonUI")
local State = require("@ed/State")

local ProjectEditing = require("@ed/stages/ProjectEditing")

local GameClosedConnection = nil

local function valuesOf<I, V>(t: { read [I]: V }): { V }
    local values = {}

    for _, v in t do
        table.insert(values, v)
    end

    return values
end

DeveloperTools.DebuggerRequestedStop:Connect(function()
    if State.CurrentStage == "ProjectPlaying" then
        State.ChangeStageTo("ProjectEditing")
    else
        warn(`Got debugger stop signal while in { State.CurrentStage }`)
    end
end)

ScriptEngine.BreakpointMoved:Connect(function(Script, OriginalLine, NewLine)
    local scriptBreakpoints = ProjectEditing.Breakpoints[Script]

    if not scriptBreakpoints then
        warn(`Breakpoint { Script } moved from { OriginalLine } -> { NewLine }, but breakpoints were never applied?`)
        return
    end

    local breakpoint = scriptBreakpoints[OriginalLine]
    scriptBreakpoints[OriginalLine] = nil
    scriptBreakpoints[NewLine] = breakpoint
    breakpoint.Line = NewLine

    DeveloperTools:SetDocumentBreakpoints(Script, valuesOf(scriptBreakpoints))
end)

function ProjectPlaying.Enter(_prev)
    PlayerInput.Cursor = Enum.Cursor.NotAllowed

    State.PlaytestRestoreState = State.Project.DataModel

    assert(State.Project.DataModel)
    -- TODO luau
    State.Project.DataModel = assert(State.Project.DataModel :: any):Duplicate()

    DeveloperTools:SetExplorerRoot(State.Project.DataModel)
    DeveloperTools:SetExplorerSelections({})

    State.Project.DataModel.VM = Configuration.PlaytestVMName
    Engine:BindDataModel(State.Project.DataModel)
    Engine:SetForegroundDataModel(State.Project.DataModel)

    ScriptEngine:CreateVM(Configuration.PlaytestVMName)
    ScriptEngine:SetVMAllowedExecutionTime(Configuration.PlaytestVMName, Preferences.ScriptAllowedExecutionTime)

    Physics:SetSimulationForcePaused(false)

    if Preferences.DebuggerEnabled then
        for scriptPath, breakpointsForScript in ProjectEditing.Breakpoints do
            for _, breakpoint in valuesOf(breakpointsForScript) do
                local realLine = ScriptEngine:SetScriptBreakpoint(
                    Configuration.PlaytestVMName,
                    scriptPath,
                    breakpoint.Line,
                    breakpoint.Enabled or breakpoint.ConditionEnabled
                )

                if realLine == -1 then
                    warn(`Failed to apply breakpoint { scriptPath }:{ breakpoint.Line } at playtest start`)
                elseif realLine ~= breakpoint.Line then
                    breakpointsForScript[breakpoint.Line] = nil
                    breakpointsForScript[realLine] = breakpoint
                    breakpoint.Line = realLine
                end
            end

            DeveloperTools:SetDocumentBreakpoints(scriptPath, valuesOf(breakpointsForScript))
        end
    else
        ScriptEngine:DetachDebuggerFromVM(Configuration.PlaytestVMName)
    end

    local projWp = State.Project.Workspace
    assert(projWp)

    if not projWp.SceneCamera then
        projWp.SceneCamera = GameObject.fromTemplate("Camera")
        projWp.SceneCamera.Parent = projWp
        projWp.SceneCamera.Name = "Camera"
        warn("Created playtest camera")
    end

    GameClosedConnection = State.Project.DataModel.Closing:Connect(function()
        assert(GameClosedConnection)

        GameClosedConnection:Disconnect()
        GameClosedConnection = nil
        State.ChangeStageTo("ProjectEditing")
    end)

    PlayerInput.Cursor = Enum.Cursor.Arrow
    return State.Project.DataModel -- Connect to the Project's datamodel `.OnFrameBegin`
end

function ProjectPlaying.Leave(_new)
    assert(State.PlaytestRestoreState)
    assert(State.Project.DataModel)

    Engine:SetForegroundDataModel(game)
    Engine:UnbindDataModel(State.Project.DataModel)

    State.Project.DataModel.Enabled = false -- trigger TreeEnabled changes -> stop playing sounds :)
    State.Project.DataModel:Destroy()
    State.Project.DataModel = State.PlaytestRestoreState
    DeveloperTools:SetExplorerRoot(State.Project.DataModel)

    ScriptEngine:CloseVM(Configuration.PlaytestVMName)
    Physics:SetSimulationForcePaused(true)

    PlayerInput.CursorMode = Enum.CursorMode.Normal
end

function ProjectPlaying.Render(DeltaTime: number)
    CommonUI.RenderMainMenuBar(DeltaTime)
    CommonUI.RenderViewport(DeltaTime)
end

return ProjectPlaying
