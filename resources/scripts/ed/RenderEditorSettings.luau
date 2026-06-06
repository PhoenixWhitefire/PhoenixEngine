-- RenderEditorSettings.luau, 04/06/2026
local Engine = game:GetService("Engine")

local SPYWARE_SETTINGS_URL = "https://github.com/PhoenixWhitefire/PhoenixEngine/issues/30"

local ExternalCodeEditor = require("@ed/ExternalCodeEditor")
local Preferences = require("@ed/Preferences")

local function renderEditorSettings()
    imgui.demowindow(true)

    local lightMode = imgui.checkbox("Light", Preferences.UseLightTheme)
    if lightMode ~= Preferences.UseLightTheme then
        imgui.stylecolors(if lightMode then "l" else "d")
    end
    Preferences.UseLightTheme = lightMode

    if imgui.windowappearing() and ExternalCodeEditor.RequestOpenSettingsMenuToConfigure then
        imgui.focuskeyboardhere()
    end

    Preferences.CodeEditorPath = imgui.inputstring("Code Editor path", Preferences.CodeEditorPath)

    Preferences.AutoLaunchCodeEditorOnProjectOpen = imgui.checkbox("Launch code editor on project open", Preferences.AutoLaunchCodeEditorOnProjectOpen)

    Preferences.ClearOutputOnProjectOpen = imgui.checkbox("Clear Output on Project open", Preferences.ClearOutputOnProjectOpen)
    Preferences.ClearOutputOnPlaytest = imgui.checkbox("Clear Output on Playtest", Preferences.ClearOutputOnPlaytest)

    Preferences.ClosingProjectSaveRequiresConfirmation = imgui.checkbox(
        "Require confirmation for save when closing Project through `File` > `Close`",
        Preferences.ClosingProjectSaveRequiresConfirmation
    )

    Preferences.AutosaveInterval = imgui.inputnumber("Autosave interval (in minutes)", Preferences.AutosaveInterval)
    Preferences.KeepGizmoInView = imgui.checkbox("Turn the camera to keep the gizmo handles in view while moving an object", Preferences.KeepGizmoInView)
    Preferences.InvertTabButtonLitColors = imgui.checkbox("Invert tab button active colors", Preferences.InvertTabButtonLitColors)
    Preferences.GitIntegration = imgui.checkbox("Git integration", Preferences.GitIntegration)

    Preferences.ScriptAllowedExecutionTime = imgui.inputnumber("Editor playtest script allowed execution time", Preferences.ScriptAllowedExecutionTime)
    imgui.setitemtooltip("This is independent of the time-out length applied for scripts in exported projects")

    Preferences.CancelPlaytestWhenScriptTimesOut = imgui.checkbox("Cancel playtests when a script times-out", Preferences.CancelPlaytestWhenScriptTimesOut)

    imgui.separator()

    imgui.text(`Your projects directory is:\n{ Preferences.ProjectsDirectory }`)
    imgui.setitemtooltip(Preferences.ProjectsDirectory)

    if imgui.button("Change") then
        local newDir = fs.promptopenfolder("Select Projects directory", Preferences.ProjectsDirectory)

        if newDir and fs.resolvepathabsolute(newDir) ~= fs.resolvepathabsolute(Preferences.ProjectsDirectory) then
            local moveSuccess, err = pcall(function()
                for p, t in fs.listdir(Preferences.ProjectsDirectory) do
                    local subst, count = p:gsub(fs.resolvepathabsolute(Preferences.ProjectsDirectory), fs.resolvepathabsolute(newDir))

                    if count == 0 then
                        error(`Failed to substitute '{fs.resolvepathabsolute(newDir)}' to replace '{fs.resolvepathabsolute(Preferences.ProjectsDirectory)}' in '{p}'`)
                    end

                    fs.move(p, subst)
                end
            end)

            if not moveSuccess then
                Engine:ShowMessageBox(
                    "Failed to move projects",
                    `Failed to move your projects from '{ fs.resolvepathabsolute(Preferences.ProjectsDirectory) }' to '{ fs.resolvepathabsolute(newDir) }'.\n\nError: { err }\n\n`
                    .. `Your Projects directory has not been updated.`,
                    "ok",
                    "error"
                )
            else
                fs.remove(Preferences.ProjectsDirectory)
                Preferences.ProjectsDirectory = newDir
            end
        end
    end

    imgui.urllink("https://youtu.be/YCjNT9qGjh4?t=7193")

    if imgui.treenode("##") then
        imgui.urllink("Configure spyware settings", SPYWARE_SETTINGS_URL)
        imgui.treepop()
    end
end

return renderEditorSettings
