-- State.luau, 30/05/2026
local Types = require("@ed/Types")

local State: { Project: Types.Project, CurrentStage: Types.Stage } = {
    Project = {
        DataModel = nil,
        Workspace = nil,
        Name = "Should not be visible",
        IsTempPath = false,
        HasGitRepository = false,
    },
    CurrentStage = "None",
}

return State
