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

local GameClosedConnection = nil

function ProjectPlaying.Enter(_prev)
    PlayerInput.Cursor = Enum.Cursor.NotAllowed

    State.PlaytestRestoreState = State.Project.DataModel

    assert(State.Project.DataModel)
    -- TODO luau
    State.Project.DataModel = assert(State.Project.DataModel :: any):Duplicate()

    DeveloperTools:SetExplorerRoot(State.Project.DataModel)
    DeveloperTools:SetExplorerSelections({})
    Physics.Simulating = true

    State.Project.DataModel.VM = Configuration.PlaytestVMName
    Engine:BindDataModel(State.Project.DataModel) -- Set the `game` and `workspace` variables to the correct values in game scripts
    ScriptEngine:CreateVM(Configuration.PlaytestVMName)
    ScriptEngine:SetVMAllowedExecutionTime(Configuration.PlaytestVMName, Preferences.ScriptAllowedExecutionTime)

    if not Preferences.DebuggerEnabled then
        ScriptEngine:DetachDebuggerFromVM(Configuration.PlaytestVMName)
    end

    local projWp = State.Project.Workspace
    assert(projWp)

    if not projWp.SceneCamera then
        projWp.SceneCamera = GameObject.new("Camera")
        projWp.SceneCamera.Parent = projWp
        projWp.SceneCamera.Name = "Camera"
    end

    GameClosedConnection = State.Project.DataModel.Closing:Connect(function()
        assert(GameClosedConnection)

        GameClosedConnection:Disconnect()
        GameClosedConnection = nil
        State.ChangeStageTo("ProjectEditing")
    end)

    Physics:SetSimulationForcePaused(false)

    PlayerInput.Cursor = Enum.Cursor.Arrow
    return State.Project.DataModel -- Connect to the Project's datamodel `.OnFrameBegin`
end

function ProjectPlaying.Leave(_new)
    assert(State.PlaytestRestoreState)
    assert(State.Project.DataModel)

    Engine:BindDataModel(game)

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
