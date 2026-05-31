-- stages/Update.luau, 29/05/2026
local Update = {}

local latestCommit = nil
local canUpdate = false
local isFetchingLatestCommitData = false
local fetchCommitHashData = nil

function Update.Enter()
    imgui.openpopup("Updater")
    canUpdate = false
    switchedToUpdater = game.Time
end

return Update
