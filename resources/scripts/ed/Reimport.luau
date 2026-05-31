-- Reimport.luau, 30/05/2026
type Watcher = {
    Models: { GameObject & EcModel },
    Disconnect: () -> (),
}

local Reimport = {
    Watchers = {} :: { [string]: Watcher },
    ModelModifiedTimes = {},
}

local AssetManager = game:GetService("AssetManager")
local Logging = game:GetService("Logging")
local History = game:GetService("History")

local Preferences = require("@ed/Preferences")

function Reimport.WatchModel(reimportable: GameObject & EcModel)
    local watcher = Reimport.Watchers[reimportable.ImportPath]

    if watcher then
        table.insert(watcher.Models, reimportable)
        return
    end

    watcher = {
        Models = { reimportable },
        Disconnect = fs.watch(reimportable.ImportPath, function(_, EventType)
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
        end)
    }
    Reimport.Watchers[reimportable.ImportPath] = watcher
end

return Reimport
