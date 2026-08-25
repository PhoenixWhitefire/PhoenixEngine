-- Reimport.luau, 30/05/2026
type Watcher = {
    Models: { GameObject & EcModel },
    Disconnect: () -> (),
}

local Reimport = {
    Watchers = {} :: { [string]: Watcher? },
    ModelModifiedTimes = {},
}

local AssetManager = game:GetService("AssetManager")
local Collections = game:GetService("Collections")
local Logging = game:GetService("Logging")
local History = game:GetService("History")

local Configuration = require("@ed/Configuration")
local ErrorMessage = require("@ed/ErrorMessage")
local Preferences = require("@ed/Preferences")

local json = phoenix.json
local fs = phoenix.fs

if fs.isfile("./editor/models.json") then
    Reimport.ModelModifiedTimes = json.parse(assert(fs.read("./editor/models.json"))) :: { [string]: number }
end

local function onReimportableTagAdded(Object: GameObject)
    if not Object:HasComponent("Model") or not Object:HasComponent("Transform") then
        return
    end

    Reimport.WatchModel(Object :: GameObject & EcModel)
end

local function onReimportableTagRemoved(Object: GameObject)
    if not Object:HasComponent("Model") or not Object:HasComponent("Transform") then
        return
    end

    local model = Object :: GameObject & EcModel
    local watcher = Reimport.Watchers[model.ImportPath]

    if not watcher then
        return
    end

    local index = table.find(watcher.Models, model)

    if not index then
        return
    end

    table.remove(watcher.Models, index)

    if #watcher.Models == 0 then
        watcher.Disconnect()
        Reimport.Watchers[model.ImportPath] = nil
    end
end

local function wrapReimportWatcher(reimportable, watcher: Watcher)
    local function reimportWatcher(_, EventType)
        if EventType ~= Enum.FileSystemEvent.Modified then
            return
        end

        if not Preferences.AutoReimportModels then
            return
        end

        assert(watcher)
        print(`Re-importing { reimportable.ImportPath }`)

        local success, importedOrError = pcall(function()
            return AssetManager:ImportModel(reimportable.ImportPath)
        end)

        if not success then
            Logging:Write(`Re-import failed: { importedOrError }`, Enum.LogType.Error)
            return
        end

        assert(typeof(importedOrError) == "GameObject")

        local action = History:TryBeginAction("Auto re-import during session")

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

        Reimport.ModelModifiedTimes[reimportable.ImportPath] = fs.lastwritten(reimportable.ImportPath)
    end

    return reimportWatcher
end

local function dummyDisconnect(): ()
end

function Reimport.WatchModel(reimportable: GameObject & EcModel)
    local watcher = Reimport.Watchers[reimportable.ImportPath]

    if watcher then
        if not table.find(watcher.Models, reimportable) then
            table.insert(watcher.Models, reimportable)
        end

        return
    end

    if not fs.isfile(reimportable.ImportPath) then
        ErrorMessage.Push(`Cannot watch { reimportable:GetFullName() } for auto re-import, file '{ reimportable.ImportPath }' not found`)
        warn(debug.traceback())
        return
    end

    watcher = {
        Models = { reimportable },
        Disconnect = dummyDisconnect
    }
    watcher.Disconnect = fs.watch(reimportable.ImportPath, wrapReimportWatcher(reimportable, watcher))

    Reimport.Watchers[reimportable.ImportPath] = watcher
end

function Reimport.CheckAll()
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
end

Collections:GetTagAddedSignal(Configuration.ReimportableModelTag):Connect(onReimportableTagAdded)
Collections:GetTagRemovedSignal(Configuration.ReimportableModelTag):Connect(onReimportableTagRemoved)

return Reimport
