-- EditorSettings.luau, 04/06/2026
local EditorSettings = {}

local Engine = game:GetService("Engine")

local SPYWARE_SETTINGS_URL = "https://github.com/PhoenixWhitefire/PhoenixEngine/issues/30"

local DefaultThemes = require("@ed/DefaultThemes")
local ErrorMessage = require("@ed/ErrorMessage")
local Preferences = require("@ed/Preferences")
local ApplyTheme = require("@ed/ApplyTheme")
local Theme = require("@ed/Theme")

local SettingsSectionHorizontalSize = 0
local LastUpdatedThemes = 0
local AvailableThemes = {
    "Light",
    "Dark",
}

local ThemesDirectory = "./editor/themes"

type ImGuiColor = {
    Name: string,
    Value: EnumImGuiColor
}
local ImGuiColors: { ImGuiColor } = {}

for k, e in Enum.ImGuiColor :: { [string]: EnumImGuiColor } do
    table.insert(ImGuiColors, {
        Name = k,
        Value = e
    })
end

table.sort(ImGuiColors, function(a: ImGuiColor, b: ImGuiColor)
    return a.Name < b.Name
end)

local function keysOf<T>(t: { [T]: any }, exclude: T?) : { T }
    local kt = {}
    for k, _ in t do
        if k ~= exclude then
            table.insert(kt, k)
        end
    end

    return kt
end

local OrderedThemeTokens = keysOf(Theme, "ImGui")
table.sort(OrderedThemeTokens)

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

local NewThemeName = ""

local function applyThemeChanges()
    fs.write(`./editor/{ Preferences.Theme }`, json.encode(Theme))
    ApplyTheme(Preferences.Theme)
end

local function renderAppearanceSection()
    Preferences.InvertTabButtonLitColors = setting("Invert tab button active colors", imgui.checkbox, Preferences.InvertTabButtonLitColors)

    local currentThemeIndex = table.find(AvailableThemes, Preferences.Theme)
    if not currentThemeIndex then
        currentThemeIndex = 1
    end

    settingPosition("Theme")
    local newThemeIndex = imgui.combo("##theme", AvailableThemes, assert(currentThemeIndex))

    if newThemeIndex ~= currentThemeIndex then
        local name = AvailableThemes[newThemeIndex]
        ApplyTheme(name)

        Preferences.Theme = name
    end

    if imgui.button("Create new theme from this") then
        imgui.openpopup("Create new theme")

        local WSX, WSY = 250, 80
        local sf = imgui.scalefactor()

        imgui.setnextwindowsize(WSX * sf, WSY * sf)
    end

    if imgui.beginpopupmodal("Create new theme", true) then
        if imgui.windowappearing() then
            imgui.focuskeyboardhere()
        end

        NewThemeName = imgui.inputstring("Name", NewThemeName)

        if imgui.button("Create", imgui.contentregionavail().x) then
            local shortPath = `themes/{ NewThemeName }.json`
            local fullPath = `./editor/{ shortPath }`

            if fs.isfile(fullPath) then
                ErrorMessage.Push("A theme with that name already exists!")
            else
                local newThemeData = {
                    ImGui = {
                        Base = Theme.ImGui.Base,
                        Overrides = {}
                    },
                    Inherit = Preferences.Theme
                }

                local s, e = pcall(function()
                    fs.write(fullPath, json.encode(newThemeData))
                end)

                if s then
                    Preferences.Theme = shortPath
                    ApplyTheme(shortPath)

                    imgui.closecurrentpopup()
                else
                    ErrorMessage.Push(`Failed to create theme file: { e }`)
                end
            end
        end

        imgui.endpopup()
    end

    imgui.separatortext("Edit theme")

    if DefaultThemes[Preferences.Theme] then
        imgui.text("You must create a copy of the default theme in order to make edits")
    else
        local currentInheritsIndex = if Theme.Inherit then table.find(AvailableThemes, Theme.Inherit) else 0
        currentInheritsIndex = if currentInheritsIndex then currentInheritsIndex + 1 else 1

        local inheritables = table.clone(AvailableThemes)
        table.insert(inheritables, 1, "<None>")

        settingPosition("Inherit")
        local newInheritsIndex = imgui.combo("##inherit", inheritables, currentInheritsIndex)

        if newInheritsIndex ~= currentInheritsIndex then
            newInheritsIndex -= 1
            Theme.Inherit = AvailableThemes[newInheritsIndex]
            applyThemeChanges()
        end

        imgui.separator()

        local BaseStyles = {
            "Light",
            "Dark",
            "Classic"
        }

        local prevBaseStyleIndex = table.find(BaseStyles, Theme.ImGui.Base) or 2

        settingPosition("Base style")
        local newBaseStyleIndex = imgui.combo("##base", BaseStyles, prevBaseStyleIndex)

        if newBaseStyleIndex ~= prevBaseStyleIndex then
            Theme.ImGui.Base = BaseStyles[newBaseStyleIndex]
            applyThemeChanges()
        end

        if imgui.collapsingheader("ImGui base colors") then
            imgui.indent(25)

            for _, col in ImGuiColors do
                imgui.pushid(`##imgui{ col.Name }`)

                settingPosition(col.Name)

                if Theme.ImGui.Overrides[col.Name] then
                    local edited = imgui.rgbaedit(`##{ col.Name }`, Theme.ImGui.Overrides[col.Name])
                    if edited then
                        applyThemeChanges()
                    end
                else
                    if imgui.button("...") then
                        Theme.ImGui.Overrides[col.Name] = imgui.getstylecolor(col.Value)
                        applyThemeChanges()
                    end
                    imgui.setitemtooltip("Override")
                end

                imgui.popid()
                imgui.separator()
            end

            imgui.indent(-25)
            imgui.newline()
        end

        for _, k in OrderedThemeTokens do
            local currentValue = Theme[k]

            settingPosition(k)
            local edited = imgui.rgbaedit(`##token{ k }`, currentValue)

            if edited then
                applyThemeChanges()
            end

            imgui.separator()
        end
    end
end

local function renderExternalToolsSection()
    if imgui.windowappearing() and EditorSettings.RequestOpenSection == "ExternalTools" then
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
    Preferences.DebuggerEnabled = setting("Debugger enabled", imgui.checkbox, Preferences.DebuggerEnabled)

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

local function updateAvailableThemes()
    LastUpdatedThemes = game.Time
    AvailableThemes = { "Light", "Dark" }

    if not fs.isdirectory("./editor") then
        fs.mkdir("./editor")
    end

    if not fs.isdirectory(ThemesDirectory) then
        fs.mkdir(ThemesDirectory)
    end

    for path, type in fs.listdir(ThemesDirectory) do
        if type ~= "f" then
            continue
        end

        local edLoc = path:find("editor/")
        local shortened = path

        if edLoc then
            shortened = path:sub(edLoc + #("editor/"), #path)
        end

        table.insert(AvailableThemes, shortened)
    end
end

function EditorSettings.Render()
    renderSectionSelectionPane()

    if imgui.windowappearing() and EditorSettings.RequestOpenSection then
        CurrentSectionId = EditorSettings.RequestOpenSection
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

    if game.Time - LastUpdatedThemes > 1 then
        updateAvailableThemes()
    end

    EditorSettings.RequestOpenSection = nil
end

EditorSettings.RequestOpenSection = nil :: keyof<typeof(Sections)>?
return EditorSettings
