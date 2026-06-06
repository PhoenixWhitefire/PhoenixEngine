-- RenderEditorSettings.luau, 04/06/2026
local Engine = game:GetService("Engine")

local SPYWARE_SETTINGS_URL = "https://github.com/PhoenixWhitefire/PhoenixEngine/issues/30"

local ExternalCodeEditor = require("@ed/ExternalCodeEditor")
local Preferences = require("@ed/Preferences")

local SettingsSectionHorizontalSize = 0

local function settingPosition(Label: string)
    imgui.text(Label)
    imgui.sameline()

    imgui.setcursorposition(vector.create(SettingsSectionHorizontalSize * 0.7, imgui.cursorposition().y))
    imgui.setnextitemwidth(imgui.contentregionavail().x)
end

local function setting<R, A...>(Label: string, Function: (string, A...) -> (R), ...: A...): R
    imgui.pushid(`##{Label}`)
    settingPosition(Label)

    local result = Function("##", ...)
    imgui.separator()

    imgui.popid()
    return result
end

local function stringSetting(Label: string, Value: string): (string, boolean)
    imgui.pushid(`##{Label}`)
    settingPosition(Label)

    local newValue, changed = imgui.inputstring("##", Value)
    imgui.separator()

    imgui.popid()
    return newValue, changed
end

local function numberSetting(Label: string, Value: number): (number, boolean)
    imgui.pushid(`##{Label}`)
    settingPosition(Label)

    local newValue, changed = imgui.inputnumber("##", Value)
    imgui.separator()

    imgui.popid()
    return newValue, changed
end

local function renderAppearanceSection()
    local lightMode = setting("Light", imgui.checkbox, Preferences.UseLightTheme)
    if lightMode ~= Preferences.UseLightTheme then
        imgui.stylecolors(if lightMode then "l" else "d")
    end

    Preferences.UseLightTheme = lightMode
    Preferences.InvertTabButtonLitColors = setting("Invert tab button active colors", imgui.checkbox, Preferences.InvertTabButtonLitColors)
end

local function renderExternalToolsSection()
    if imgui.windowappearing() and ExternalCodeEditor.RequestOpenSettingsMenuToConfigure then
        imgui.focuskeyboardhere()
    end

    Preferences.CodeEditorPath = stringSetting("Code Editor path", Preferences.CodeEditorPath)
    Preferences.AutoLaunchCodeEditorOnProjectOpen = setting("Launch code editor on project open", imgui.checkbox, Preferences.AutoLaunchCodeEditorOnProjectOpen)

    Preferences.GitIntegration = setting("Git integration", imgui.checkbox, Preferences.GitIntegration)
end

local function renderProjectSection()
    Preferences.ClosingProjectSaveRequiresConfirmation = setting(
        "Require confirmation for save\nwhen closing Project through `File` > `Close`",
        imgui.checkbox,
        Preferences.ClosingProjectSaveRequiresConfirmation
    )

    Preferences.AutosaveInterval = numberSetting("Autosave interval (in minutes)", Preferences.AutosaveInterval)
    Preferences.KeepGizmoInView = setting("Turn the camera to keep the gizmo handles in view while moving an object", imgui.checkbox, Preferences.KeepGizmoInView)

    Preferences.ClearOutputOnProjectOpen = setting("Clear Output on Project open", imgui.checkbox, Preferences.ClearOutputOnProjectOpen)
    Preferences.ClearOutputOnPlaytest = setting("Clear Output on Playtest", imgui.checkbox, Preferences.ClearOutputOnPlaytest)

    Preferences.ScriptAllowedExecutionTime = numberSetting("Editor playtest script allowed execution time", Preferences.ScriptAllowedExecutionTime)
    imgui.setitemtooltip("This is independent of the time-out length applied for scripts in exported projects")

    Preferences.CancelPlaytestWhenScriptTimesOut = setting("Cancel playtests when a script times-out", imgui.checkbox, Preferences.CancelPlaytestWhenScriptTimesOut)

    imgui.newline()

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

type Section = {
    Name: string?,
    Render: () -> ()
}

local Sections: { [string]: Section } = {
    Appearance = {
        Render = renderAppearanceSection
    },
    ExternalTools = {
        Name = "External tools",
        Render = renderExternalToolsSection,
    },
    Project = {
        Render = renderProjectSection,
    },
}

local SectionOrder: { keyof<typeof(Sections)> } = {
    "Project",
    "ExternalTools",
    "Appearance",
}

local CurrentSectionId: keyof<typeof(Sections)> = "Project"

local function renderSectionSelectionPane()
    if not imgui.beginchild("SectionSelection", imgui.contentregionavail() * vector.create(0.2, 1), "b") then
        imgui.endchild()
        return
    end

    for _, sectionId in SectionOrder do
        local section = Sections[sectionId]
        local name = section.Name or sectionId
        imgui.treenode(name, "lf | fw | np" .. if CurrentSectionId == sectionId then " | sl" else "")

        if imgui.itemclicked() then
            CurrentSectionId = sectionId
        end
    end

    imgui.endchild()
    return
end

local function renderEditorSettings()
    renderSectionSelectionPane()

    if imgui.windowappearing() and ExternalCodeEditor.RequestOpenSettingsMenuToConfigure then
        CurrentSectionId = "ExternalTools"
    end

    imgui.sameline()
    local cra = imgui.contentregionavail()
    SettingsSectionHorizontalSize = cra.x

    if not imgui.beginchild("SettingsSection", cra, "b", "h") then
        imgui.endchild()
        return
    end

    local currentSection = Sections[CurrentSectionId]
    currentSection.Render()

    imgui.endchild()
end

return renderEditorSettings
