-- Stages.luau, 31/05/2026
local Stages = {
    None = require("@self/None"),
    SelectProject = require("@self/SelectProject"),
    ProjectEditing = require("@self/ProjectEditing"),
    ProjectPlaying = require("@self/ProjectPlaying"),
}

type StagesTy = typeof(Stages)

Stages = Stages :: { [keyof<StagesTy>]: index<StagesTy, keyof<StagesTy>> }
return Stages
