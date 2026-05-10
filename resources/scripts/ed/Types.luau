-- ed/Types.luau, 23/10/2025
export type Stage = "None" | "SelectProject" | "ProjectEditing" | "ProjectPlaying" | "Update"

export type ToolbarButton = {
    Text: string,
    Icon: string?,
    Enabled: boolean,
    Lit: boolean?,
    IsSeparator: boolean?,
    OnClick: (ToolbarButton) -> ()
}

export type Plugin = {
    Name: string,
    Description: string?,
    Icon: string?,
    Path: string,
    ParentPath: string?,
    ActiveStages: { Stage },
    ToolbarButtons: { ToolbarButton },

    OnLoad: ((Plugin) -> ())?,
    OnShutdown: ((Plugin) -> ())?,
    OnEditorStageChanged: ((Plugin, Stage, Stage) -> ())?,
    OnUpdate: ((Plugin, number) -> ())?
}

export type Project = {
    DataModel: (GameObject & EcDataModel)?,
    Workspace: (GameObject & EcWorkspace)?,
    Name: string,
    IsTempPath: boolean,
    HasGitRepository: boolean,
}

return true
