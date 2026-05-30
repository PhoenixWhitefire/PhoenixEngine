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

local Configuration = require("../Configuration")
local Preferences = require("../Preferences")
local CommonUI = require("../CommonUI")
local Reimport = require("../Reimport")
local Types = require("../Types")

local ProjectClosedByDataModelConnection = nil

function ProjectEditing.Enter(Editor: { Project: Types.Project, ChangeStageTo: (Types.Stage) -> () }, PreviousStage: Types.Stage)
    assert(Editor.Project.DataModel)
    assert(Editor.Project.Workspace)

    DeveloperTools:SetExplorerRoot(Editor.Project.DataModel)
    DeveloperTools:SetExplorerSelections({})
    Physics.Simulating = false
    workspace.SceneCamera = Editor.Project.Workspace.SceneCamera
    workspace.SceneCamera.UseSimpleController = true

    if PreviousStage == "SelectProject" then
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

        ProjectClosedByDataModelConnection = Editor.Project.DataModel.Closing:Connect(function()
            assert(ProjectClosedByDataModelConnection)

            ProjectClosedByDataModelConnection:Disconnect()
            ProjectClosedByDataModelConnection = nil
            Editor.ChangeStageTo("SelectProject")
        end)

        for _, name in Preferences.DevToolsOpen do
            DeveloperTools:SetToolEnabled(name, true)
        end
    end

    return nil
end

local ShowedAutosaveDelayedMessage = false

function ProjectEditing.Render(Editor: { LastAutosaved: number, SaveProject: () -> (), SetErrorNotification: (string) -> () }, DeltaTime: number)
    assert(typeof(DeltaTime) == "number")

    CommonUI.RenderMainMenuBar(DeltaTime)
    CommonUI.RenderViewport(DeltaTime)

    if (game.Time - Editor.LastAutosaved) / 60 > Preferences.AutosaveInterval then
        if not History.CurrentActionName then
            Editor.SaveProject()
            print("Autosaved")
            Editor.LastAutosaved = game.Time
            ShowedAutosaveDelayedMessage = false

        elseif not ShowedAutosaveDelayedMessage then
            Editor.SetErrorNotification(`Autosave has been delayed until the { History.CurrentActionName } action finishes`)
            ShowedAutosaveDelayedMessage = true
        end
    end
end

function ProjectEditing.Leave(Editor: { Log: {}, Project: Types.Project, SaveProject: () -> () }, NextStage: Types.Stage)
    if NextStage == "ProjectPlaying" then
        if Preferences.ClearOutputOnPlaytest then
            table.clear(Editor.Log)
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

    assert(Editor.Project.DataModel)
    if not Preferences.ClosingProjectSaveRequiresConfirmation
        or Engine:ShowMessageBox("Save?", "Save datamodel before closing the current project?", "yesno", "question", 1) == 1
    then
        Editor.SaveProject()
    end

    Editor.Project.DataModel:Destroy()
    Editor.Project.Workspace = nil
    Editor.Project.Name = "Should not be visible"
    Editor.Project.IsTempPath = true
    Editor.Project.DataModel = nil

    Engine:SaveConfig()
    ScriptEngine:CloseVM(Configuration.GameEditingVM)
end

return ProjectEditing
