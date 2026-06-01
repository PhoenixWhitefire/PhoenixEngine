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

local Configuration = require("@ed/Configuration")
local ErrorMessage = require("@ed/ErrorMessage")
local Preferences = require("@ed/Preferences")
local CommonUI = require("@ed/CommonUI")
local Reimport = require("@ed/Reimport")
local Output = require("@ed/Output")
local State = require("@ed/State")
local Types = require("@ed/Types")

local ProjectClosedByDataModelConnection = nil
local EditorCam = workspace.SceneCamera

type SavedEditorCamera = {
    FieldOfView: number,
    Position: {
        X: number,
        Y: number,
        Z: number
    },
    Rotation: {
        X: number,
        Y: number,
        Z: number
    }
}

local function loadEditorCameraTransform()
    local savedCamTrans: SavedEditorCamera = {
        FieldOfView = 90,
        Position = {
            X = 0,
            Y = 0,
            Z = 0
        },
        Rotation = {
            X = 0,
            Y = 0,
            Z = 0
        }
    }

    if fs.isfile("@project/phoenix.editor") then
        local editordata = json.parse(assert((fs.read("@project/phoenix.editor")))) :: any
        savedCamTrans = (editordata.SavedEditorCamera :: SavedEditorCamera) or savedCamTrans
    end

    EditorCam.FieldOfView = savedCamTrans.FieldOfView
    EditorCam.Transform = Matrix.translated(savedCamTrans.Position.X, savedCamTrans.Position.Y, savedCamTrans.Position.Z)
                            * Matrix.rotated(savedCamTrans.Rotation.X, savedCamTrans.Rotation.Y, savedCamTrans.Rotation.Z)
end

local function saveEditorCameraTransform()
    local position = EditorCam.Transform.Position
    local rotation = EditorCam.Transform:ExtractAngles()

    local savedCamTrans: SavedEditorCamera = {
        FieldOfView = EditorCam.FieldOfView,
        Position = {
            X = position.x,
            Y = position.y,
            Z = position.z
        },
        Rotation = {
            X = rotation.x,
            Y = rotation.y,
            Z = rotation.z
        }
    }

    local editordata = {}
    if fs.isfile("@project/phoenix.editor") then
        editordata = json.parse(assert((fs.read("@project/phoenix.editor")))) :: any
    end

    editordata.SavedEditorCamera = savedCamTrans
    fs.write("@project/phoenix.editor", json.encode(editordata))
end

function ProjectEditing.Enter(PreviousStage: Types.Stage)
    assert(State.Project.DataModel)
    assert(State.Project.Workspace)

    DeveloperTools:SetExplorerRoot(State.Project.DataModel)
    DeveloperTools:SetExplorerSelections({})
    Physics.Simulating = false

    if PreviousStage == "SelectProject" then
        loadEditorCameraTransform()

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
end

function ProjectEditing.Leave(NextStage: Types.Stage)
    saveEditorCameraTransform()

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
