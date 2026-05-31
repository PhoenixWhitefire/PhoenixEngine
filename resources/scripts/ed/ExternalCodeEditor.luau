-- ExternalCodeEditor.luau, 31/05/2026
local ExternalCodeEditor = {
    RequestOpenSettingsMenuToConfigure = false,
}

local Engine = game:GetService("Engine")

local Preferences = require("@ed/Preferences")

function ExternalCodeEditor.Open(ProjectName: string)
    if not Preferences.CodeEditorPath or #Preferences.CodeEditorPath == 0 then
        Engine:ShowMessageBox(
            "Configure Code Editor",
            "You have not configured what Code Editor you would like the Editor to use.\n"
            .. "You can set the command to launch your desired Code Editor in Editor Settings.\n"
            .. "Look up the relevant command for your Code Editor. For example, the command for Visual Studio Code is 'code'."
        )

        ExternalCodeEditor.RequestOpenSettingsMenuToConfigure = true
    else
        ExternalCodeEditor.RequestOpenSettingsMenuToConfigure = false

        local result = fs.execute(`{ Preferences.CodeEditorPath } "{ fs.resolvepathabsolute(Preferences.ProjectsDirectory) }/{ ProjectName }"`)

        if #result > 1 then
            warn("Open code editor result: ", result)
        end
    end
end

return ExternalCodeEditor
