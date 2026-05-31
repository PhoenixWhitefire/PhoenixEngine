-- Stages.luau, 31/05/2026
local Stages = {
    SelectProject = require("@ed/stages/SelectProject"),
    ProjectEditing = require("@ed/stages/ProjectEditing"),
    ProjectPlaying = require("@ed/stages/ProjectPlaying"),
}

type StagesTy = typeof(Stages)

Stages = Stages :: { [keyof<StagesTy>]: index<StagesTy, keyof<StagesTy>> }
return Stages
