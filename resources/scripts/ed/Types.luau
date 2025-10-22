-- ed/Types.luau, 23/10/2025
export type Stage = "None" | "SelectProject" | "ProjectEditing" | "ProjectPlaying"

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
    ActiveStages: { Stage },
    ToolbarButtons: { ToolbarButton },

    OnLoad: ((Plugin) -> ())?,
    OnShutdown: ((Plugin) -> ())?,
    OnEditorStageChanged: ((Plugin, Stage, Stage) -> ())?,
    OnUpdate: ((Plugin, number) -> ())?
}

return true
