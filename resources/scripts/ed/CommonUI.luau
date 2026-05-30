-- CommonUI.luau, 30/05/2026
local CommonUI = {}

local ScriptEngine = game:GetService("ScriptEngine")
local AssetManager = game:GetService("AssetManager")
local PlayerInput = game:GetService("PlayerInput")
local Network = game:GetService("Network")
local History = game:GetService("History")
local Engine = game:GetService("Engine")

local Configuration = require("@ed/Configuration")
local ErrorMessage = require("@ed/ErrorMessage")
local Preferences = require("@ed/Preferences")
local State = require("@ed/State")
local Types = require("@ed/Types")
local Git = require("@ed/Git")

local SPYWARE_SETTINGS_URL = "https://github.com/PhoenixWhitefire/PhoenixEngine/issues/30"

local HoveringOverMenuBar = false
local HoveringOverRibbonTab = false
local RibbonPopupDistance = 16

local function findEnginePath(): string?
    local commandPath = Engine:GetCliArguments()[1]

    if fs.isfile("./" .. commandPath) then
        return "./" .. commandPath
    end

    if fs.isfile(commandPath) then
        return commandPath
    end

    local choice = Engine:ShowMessageBox("Export", "The Engine executable could not be detected automatically. Please select the Engine executable file.", "okcancel", "warning", 1)
    if choice == 0 then
        return nil
    else
        return fs.promptopen()[1]
    end
end

local function compileScriptBytecodeRecursive(dir)
    for path, ty in fs.listdir(dir) do
        if ty == "f" then
            if path:find(".config.luau") then
                continue
            end

            local extLoc = path:find(".luau")
            if not extLoc or extLoc ~= #path - #(".luau") + 1 then
                continue
            end

            local contents, err = fs.read(path)
            if not contents then
                error(`Error compiling bytecode for '{ path }': { err }`)
            end

            local bytecode = ScriptEngine:CompileToBytecode(
                contents,
                2, -- optimization level
                1  -- debug level
            )
            fs.write(path .. "c", bytecode) -- file extension is `.luauc`

            fs.remove(path) -- remove original source code
        else
            compileScriptBytecodeRecursive(path)
        end
    end
end

local function exportProject(Editor: { SaveProject: () -> () })
    Editor.SaveProject()

    local enginePath = findEnginePath()

    if not enginePath then
        print("Export cancelled")
        return
    end

    imgui.openpopup("Export Project")
    local exportName = State.Project.Name
    local exportDir = fs.promptopenfolder("Choose export directory")
    local exportTo = `{exportDir}/{exportName}`

    if not exportDir then
        print("No export directory selected, export cancelled")
        return
    end

    local keepOnlyScriptBytecode = false
    local prevExportTo = ""

    while true do
        if not imgui.beginpopupmodal("Export Project", true) then
            print("Export cancelled - Popup closed")
            return
        end

        exportName = imgui.inputstring("Export As", exportName)
        if exportName:find("[^%w%s]") then
            imgui.text("Only alphanumeric characters are permitted")
        end

        imgui.text(`Export directory: {exportDir}`)
        imgui.sameline()

        if imgui.button("...") then
            exportDir = fs.promptopenfolder("Change export directory")
        end

        keepOnlyScriptBytecode = imgui.checkbox("Export scripts as bytecode", keepOnlyScriptBytecode)

        exportTo = `{exportDir}/{exportName}`

        imgui.separator()

        imgui.text("Your project will export to:")
        imgui.pushstylecolor(0, { 1, 1, 1, 1 })
        imgui.text(exportTo)
        imgui.popstylecolor()

        if prevExportTo ~= exportTo then
            if fs.isdirectory(exportTo) then
                imgui.text("This directory already exists!")
            end
        else
            prevExportTo = exportTo
        end

        if imgui.button("Cancel") then
            print("Export cancelled - Cancel button clicked in Export dialogue")
            imgui.endpopup()
            return
        end

        imgui.sameline()

        local cax = imgui.contentregionavail().x
        local cpos = imgui.cursorposition()
        imgui.setcursorposition(cpos.x + cax - 50, cpos.y)

        if imgui.button("Export") then
            print("Export button clicked")

            if fs.isdirectory(exportTo) then
                local chosenOption = Engine:ShowMessageBox(
                    "Export will overwrite",
                    `The directory:\n\n{ exportTo }\n\nalready exists. Are you sure you want to overwrite it? This will erase all of its content!`,
                    "yesno",
                    "warning"
                )

                if chosenOption == 0 then
                    print("Chose not to overwrite")
                else
                    print("Removing directory to overwrite with export -", exportTo)
                    fs.remove(exportTo)

                    imgui.endpopup()
                    break
                end
            else
                imgui.closecurrentpopup()
                imgui.endpopup()
                break
            end
        end

        imgui.endpopup()
        game.OnFrameBegin:WaitUntil()
    end

    print("Exporting")

    fs.mkdir(exportTo)
    fs.copy("@project", exportTo)
    fs.remove(`{ exportTo }/phoenix.editor`)

    fs.copy(enginePath, `{exportTo}/{State.Project.Name}{if enginePath:find(".exe") then ".exe" else ""}`)

    for _, a in Configuration.CoreAssets do
        local path = `{ exportTo }/resources/{ a }`
        if fs.isfile(path) then
            fs.remove(path)
            fs.copy(`./resources/{ a }`, path)
        end
    end

    compileScriptBytecodeRecursive(exportTo .. "/resources/scripts")

    if enginePath:find("x64/Debug") and enginePath:find(".exe") then
        Engine:ShowMessageBox(
            "Windows Debug build",
            "Windows Debug builds cannot be distributed without additional dependencies"
        )
    end

    print("Export done")
end

local function getCenteredX(cursorx: number, cxavail: number, elementWidth: number)
    return cursorx + cxavail / 2 - elementWidth / 2
end

local latestCommit = nil
local canUpdate = false
local isFetchingLatestCommitData = false
local fetchCommitHashData = nil

local function printTable<K, V>(t: { [K]: V }, depth: string, n: string?, printFun: <T>(...T) -> ())
    printFun = printFun or print

    depth = depth or ""

    printFun(depth .. (if n then n .. " =" else ""), tostring(t), "{")
    for k, v in t do
        if typeof(v) ~= "table" then
            printFun(depth .. " ", k, "=", v)
        else
            printTable(v, depth .. "  ", tostring(k), printFun)
        end
    end
    printFun(depth .. "}")
end

local function getLatestGitHubCommit()
    local data = Network:MakeHttpRequestAsync({
        Url = "https://api.github.com/repos/PhoenixWhitefire/PhoenixEngine/commits/main",
        Headers = {
            ["User-Agent"] = "PhoenixEngine/Editor" -- GitHub requires a User Agent for their APIs
        }
    })

    if data.Ok then
        local response = json.parse(data.Body)
        assert(typeof(response) == "table", "response from GitHub API was not a table")

        local commit = (response :: { [string]: JsonSerializable }).sha
        assert(typeof(commit) == "string", ".sha was not a string")

        latestCommit = commit

        if latestCommit ~= Engine.CommitHash then
            canUpdate = true
        end
    else
        -- TODO luau cast
        printTable(data :: any, "", nil, warn)
        fetchCommitHashData = data
    end
end

local function textCentered(text, cursor, cxavail)
    imgui.setcursorposition(getCenteredX(cursor.x, cxavail, imgui.textsize(text).x), cursor.y)
    imgui.text(text)
end

local wasSettingsOpen = false
local OpenGitCommitMenu = false
local GitStatusResult = ""
local GitDiffResult = ""
local GitCommitMessage = ""

local function getGitStatus()
    if Git.Available then
        Git.StatusResult = fs.execute(`git -C "{ fs.resolvepath("@project") }" status`)
        Git.DiffResult = fs.execute(`git -C "{ fs.resolvepath("@project") }" diff`)
    else
        Git.StatusResult = "Git is not available on your system, please ensure it is installed correctly."
    end
end

function CommonUI.RenderMainMenuBar(dt: number)
    if ErrorMessage.Queue[1] then
        local front = ErrorMessage.Queue[1]

        if front.TimeRemaining > 0 then
            imgui.settooltip(front.Message)

            -- after e.g. trying to import a model, the DT will be very high
            if dt < 0.5 then
                front.TimeRemaining -= dt
            end
        else
            table.remove(ErrorMessage.Queue, 1)
        end
    end

    local sf = imgui.scalefactor()
    local pushedCol = State.CurrentStage == "ProjectPlaying"

    if pushedCol then
        imgui.pushstylecolor(13, {0, 56/270, 15/270, 1})
    end

    local render = imgui.beginmainmenubar()

    if pushedCol then
        imgui.popstylecolor()
    end

    if not render then
        return
    end

    local mainMenuCursorY = PlayerInput:GetCursorPosition().y
    HoveringOverMenuBar = mainMenuCursorY < RibbonPopupDistance and PlayerInput.CursorMode == Enum.CursorMode.Normal

    local openAboutWindow = false
    local openSettingsWindow = false

    if imgui.beginmenu("File") then
        if State.CurrentStage == "ProjectEditing" then
            if imgui.menuitem("Undo", History.CanUndo) then
                History:Undo()
            end
            if not History.CanUndo then
                imgui.setitemtooltip(History:GetCannotUndoReason())
            end

            if imgui.menuitem("Redo", History.CanRedo) then
                History:Redo()
            end
            if not History.CanRedo then
                imgui.setitemtooltip(History:GetCannotRedoReason())
            end

            if imgui.menuitem("Open in Code Editor") then
                task.defer(Editor.OpenCodeEditor)
            end

            if imgui.menuitem("Open Project folder") then
                task.defer(function()
                    if Engine.TargetPlatform == "Window" then
                        fs.execute(`start "{ fs.resolvepathabsolute("@project") }"`)
                    else
                        fs.execute(`xdg-open "{ fs.resolvepathabsolute("@project") }"`)
                    end
                end)
            end
        end

        if imgui.menuitem("Save", State.CurrentStage == "ProjectEditing") then
            Editor.SaveProject()
        end

        if Editor.CurrentStage == "ProjectEditing" then
            if imgui.menuitem("Project Settings") then
                local conf = json.parse(assert((fs.read("@project/phoenix.conf")))) :: any

                task.defer(function()
                    imgui.openpopup("Project Settings")

                    local newProjectName = State.Project.Name

                    while true do
                        if not imgui.beginpopupmodal("Project Settings", true) then
                            break
                        end

                        newProjectName = imgui.inputstring("Project Name", newProjectName)

                        if newProjectName:match("[^%w%s]") then
                            imgui.text("Only a-z, A-Z, 0-9 (alphanumeric) is allowed")
                        end

                        if newProjectName ~= State.Project.Name and imgui.button("Save new name") then
                            local buttonSelection = Engine:ShowMessageBox("Rename project?", "Are you sure you want to rename the project?", "yesno")

                            if buttonSelection == 1 then
                                local prevName = State.Project.Name

                                fs.rename(`{ Preferences.ProjectsDirectory }/{ State.Project.Name }`, newProjectName)
                                fs.definealias("project", `{ Preferences.ProjectsDirectory }/{ newProjectName }`)

                                State.Project.Name = newProjectName

                                local conf = json.parse(assert(fs.read("@project/phoenix.conf")))
                                assert(typeof(conf) == "table")
                                -- TODO: fix cast
                                conf["GameTitle" :: string] = newProjectName

                                fs.write(
                                    "@project/phoenix.conf",
                                    json.encode(conf)
                                )

                                table.remove(Editor.RecentProjects, table.find(Editor.RecentProjects, prevName))
                                Editor.SaveToRecents(newProjectName)
                            else
                                newProjectName = State.Project.Name
                            end
                        end

                        imgui.text("Project icon:")

                        if imgui.imagebutton("Icon", "@project/icon.png", vector.create(150, 150)) then
                            local buttonSelection = Engine:ShowMessageBox(
                                "Change icon?",
                                "Change the project icon? Note that the original icon file will be overwritten!",
                                "yesno",
                                "question"
                            )

                            if buttonSelection == 1 then
                                task.defer(function()
                                    local path = fs.promptopen(nil, { "*.png" , "*.jpg", "*.jpeg" }, "Images")[1]

                                    if path then
                                        fs.remove("@project/icon.png")
                                        fs.copy(path, "@project/icon.png")
                                        AssetManager:UnloadTexture("@project/icon.png") -- force the texture to be reloaded
                                    else
                                        print("No icon selected")
                                    end
                                end)
                            end
                        end

                        conf.DefaultVMAllowedExecutionTime = imgui.inputnumber("Exported script allowed execution time", conf.DefaultVMAllowedExecutionTime or 30)

                        if imgui.button("Save") then
                            fs.write("@project/phoenix.conf", json.encode(conf))
                        end

                        imgui.endpopup()

                        game.OnFrameBegin:WaitUntil()
                    end
                end)
            end

            if Preferences.GitIntegration then
                if imgui.beginmenu("Git", Git.Available) then
                    if not State.Project.HasGitRepository then
                        if imgui.menuitem("Initialize repository") then
                            task.defer(function()
                                print(fs.execute(`git -C "{ fs.resolvepath("@project") }" init`))
                                State.Project.HasGitRepository = fs.isdirectory("@project/.git")

                                if State.Project.HasGitRepository then
                                    fs.write("@project/.gitignore", "# Generated by Phoenix Engine Editor\n\nphoenix.editor\n")
                                end
                            end)
                        end
                    else
                        if imgui.menuitem("Commit") then
                            task.defer(function()
                                Editor.SaveProject()
                                getGitStatus()
                                OpenGitCommitMenu = true
                            end)
                        end
                    end

                    imgui.endmenu()
                end

                if not Git.Available then
                    imgui.setitemtooltip("Git was not detected on your system, please ensure it is installed correctly")
                end
            end

            if imgui.menuitem("Export") then
                task.defer(exportProject, Editor)
            end

            if imgui.menuitem("Close") then
                -- TODO luau cast
                task.defer(Editor.ChangeStageTo, "SelectProject" :: "SelectProject")
            end
        end

        if imgui.menuitem("Open", Editor.CurrentStage == "SelectProject") then
            SavedProjectsCache = nil
            task.defer(openProject, nil) -- TODO type error when `nil` is not supplied
        end

        if imgui.menuitem("New", CurrentStage == "SelectProject") then
            task.defer(createNewProject)
        end

        if imgui.menuitem("About") then
            openAboutWindow = true
        end

        if imgui.menuitem("Editor Settings") then
            openSettingsWindow = true
        end

        imgui.endmenu()
    end

    if OpenGitCommitMenu then
        imgui.openpopup("Commit")
        OpenGitCommitMenu = false
        GitCommitMessage = ""
    end

    if imgui.beginpopupmodal("Commit", true) then
        imgui.text(GitStatusResult)
        imgui.separator()

        if GitStatusResult:find("nothing to commit") then
            imgui.text("No changes to commit.")
        else
            local diffOpen = imgui.treenode("Diff", "dlf | fw")

            if diffOpen then
                imgui.text(GitDiffResult)
                imgui.treepop()
            end

            --local tsy = imgui.textsize("").y

            local cavail = imgui.contentregionavail()
            local cxavail = cavail.x
            local cursor = imgui.cursorposition()

            --imgui.setcursorposition(vector.create(cursor.x, cavail.y + tsy * 4 + 20))
            --imgui.dummy()

            imgui.text("All files will be committed automatically.\nCommit message:")
            imgui.setnextitemwidth(cxavail)
            GitCommitMessage = imgui.inputstring("##CommitMessage", GitCommitMessage)

            if #GitCommitMessage == 0 then
                imgui.text("Please enter a commit message.")
            end

            cursor = imgui.cursorposition()
            local buttonText = "Commit"
            imgui.setcursorposition(getCenteredX(cursor.x, cxavail, imgui.textsize(buttonText).x), cursor.y + 10 * sf)

            if imgui.button(buttonText) then
                task.defer(function()
                    GitCommitMessage = string.gsub(GitCommitMessage, '"', '\\"')
                    print(fs.execute(`git -C "{ fs.resolvepath("@project") }" add .`))
                    print(fs.execute(`git -C "{ fs.resolvepath("@project") }" commit -m "{ GitCommitMessage }" -a`))
                    getGitStatus()
                end)
            end
        end

        imgui.endpopup()
    end

    if CurrentStage ~= "SelectProject" then
        imgui.separator()

        WipTabButtonHovering = nil
        WindowHovering = false

        for i, tab in RibbonTabs do
            if imgui.menuitem(if CurrentRibbonTab == i then `[{tab.Name}]` else tab.Name) then
                CurrentRibbonTab = i
            end
        end

        if (HoveringOverMenuBar or HoveringOverRibbonTab or TabButtonHovering ~= nil or WindowHovering) and not PlayerInput:IsMouseButtonPressed(Enum.MouseButton.Right) then
            local windowWidth = Renderer.Resolution.x

            local TAB_HEIGHT = 175 * sf

            imgui.setnextwindowposition(0, 16 * sf)
            imgui.setnextwindowsize(windowWidth, TAB_HEIGHT)

            imgui.begin("RibbonTabContents", "nt | nr | nm | nc | ns | h")
            HoveringOverRibbonTab = imgui.windowhovered() or imgui.anyitemactive()
            RibbonTabs[CurrentRibbonTab].Renderer(dt)
            imgui.endw()

            RibbonPopupDistance = TAB_HEIGHT -- fixes ribbon closing when enabling tools
        else
            RibbonPopupDistance = 16 * sf
        end

        TabButtonHovering = WipTabButtonHovering
    end

    imgui.endmainmenubar()

    if openAboutWindow then
        imgui.openpopup("About")

        local ws = Renderer.Resolution

        local ABOUT_SIZE_X, ABOUT_SIZE_Y = 375 * sf, 340 * sf

        imgui.setnextwindowposition((ws.x - ABOUT_SIZE_X) / 2, (ws.y - ABOUT_SIZE_Y) / 2);
        imgui.setnextwindowsize(ABOUT_SIZE_X, ABOUT_SIZE_Y);
    end

    if imgui.beginpopupmodal("About", true) then
        local cxavail = imgui.contentregionavail().x
        local cursor = imgui.cursorposition()

        local LOGO_SIZE = 150 * sf

        imgui.setcursorposition(getCenteredX(cursor.x, cxavail, LOGO_SIZE), cursor.y)
        imgui.image(LOGO_IMAGE, vector.create(LOGO_SIZE, LOGO_SIZE))

        cursor += vector.create(0, 160 * sf)

        local tts = imgui.textsize("")

        textCentered("Phoenix Engine Editor", cursor, cxavail)
        cursor += vector.create(0, tts.y + 2 * sf)

        textCentered(Engine.Version, cursor, cxavail)
        cursor += vector.create(0, tts.y + 15 * sf)

        imgui.setcursorposition(getCenteredX(cursor.x, cxavail, imgui.textsize(GITHUB_URL).x), cursor.y)
        imgui.urllink(GITHUB_URL)
        cursor += vector.create(0, tts.y + 2 * sf)

        local commitText = `{ Engine.CommitTag } ({ Engine.CommitHash:sub(1, 8) })`

        imgui.setcursorposition(getCenteredX(cursor.x, cxavail, imgui.textsize(commitText).x), cursor.y)
        imgui.urllink(commitText, `{ GITHUB_URL }/commit/{ Engine.CommitHash }`)
        cursor += vector.create(0, tts.y + 16 * sf)

        if not isFetchingLatestCommitData and not latestCommit then
            isFetchingLatestCommitData = true
            task.defer(getLatestGitHubCommit)
        end

        if latestCommit then
            if latestCommit ~= Engine.CommitHash then
                textCentered("There is a newer version available.", cursor, cxavail)
                cursor += vector.create(0, tts.y + 2 * sf)

                textCentered(`Latest: { latestCommit:sub(1, 8) }`, cursor, cxavail)
                cursor += vector.create(0, tts.y + 4 * sf)

                local buttonText = "Download"
                imgui.setcursorposition(getCenteredX(cursor.x, cxavail, imgui.textsize(buttonText).x), cursor.y)

                if canUpdate and imgui.button(buttonText) then
                    if CurrentStage ~= "SelectProject" then
                        Engine:ShowMessageBox("Update", "You must close the current project to update")
                    elseif fs.isfile("./update.zip") then
                        Engine:ShowMessageBox("Update", "Delete or rename the 'update.zip' file in the Editor's installation directory to proceed", "ok", "error")
                    else
                        imgui.closecurrentpopup()
                        changeStageTo("Update")
                    end
                end
            else
                local urutdText = "You are on the latest available version."
                local urutdtsx = imgui.textsize(urutdText).x

                imgui.setcursorposition(getCenteredX(cursor.x, cxavail, urutdtsx), cursor.y + 32 * sf)
                imgui.text(urutdText)
            end
        elseif fetchCommitHashData then
            textCentered("Failed to check for updates.", cursor, cxavail)
            cursor += vector.create(0, tts.y + 2 * sf)
            textCentered(`Status: { fetchCommitHashData.Status }`, cursor, cxavail)
            cursor += vector.create(0, tts.y + 2 * sf)
            textCentered(`Error: { fetchCommitHashData.Body }`, cursor, cxavail)
            imgui.setitemtooltip(fetchCommitHashData.Body)
        end

        imgui.endpopup()
    end

    if openSettingsWindow or openSettingsMenuToConfigureCodeEditor then
        imgui.openpopup("Editor Settings")

        local SETTINGS_SIZE_X, SETTINGS_SIZE_Y = 420 * sf, 140 * sf

        local ws = Renderer.Resolution

        imgui.setnextwindowposition((ws.x - SETTINGS_SIZE_X) / 2, (ws.y - SETTINGS_SIZE_Y) / 2)
        imgui.setnextwindowsize(SETTINGS_SIZE_X, SETTINGS_SIZE_Y)
    end

    if imgui.beginpopupmodal("Editor Settings", true) then
        local lightMode = imgui.checkbox("Light", EditorPreferences.UseLightTheme)
        if lightMode ~= EditorPreferences.UseLightTheme then
            imgui.stylecolors(if lightMode then "l" else "d")
        end
        EditorPreferences.UseLightTheme = lightMode

        if openSettingsMenuToConfigureCodeEditor and not didFocusCodeEditorSetting then
            imgui.focuskeyboardhere()
            didFocusCodeEditorSetting = true
        end

        EditorPreferences.CodeEditorPath = imgui.inputstring("Code Editor path", EditorPreferences.CodeEditorPath or "")

        EditorPreferences.AutoLaunchCodeEditorOnProjectOpen = imgui.checkbox("Launch code editor on project open", EditorPreferences.AutoLaunchCodeEditorOnProjectOpen)

        EditorPreferences.ClearOutputOnProjectOpen = imgui.checkbox("Clear Output on Project open", EditorPreferences.ClearOutputOnProjectOpen)
        EditorPreferences.ClearOutputOnPlaytest = imgui.checkbox("Clear Output on Playtest", EditorPreferences.ClearOutputOnPlaytest)

        EditorPreferences.ClosingProjectSaveRequiresConfirmation = imgui.checkbox(
            "Require confirmation for save when closing Project through `File` > `Close`",
            EditorPreferences.ClosingProjectSaveRequiresConfirmation
        )

        EditorPreferences.AutosaveInterval = imgui.inputnumber("Autosave interval (in minutes)", EditorPreferences.AutosaveInterval)
        EditorPreferences.KeepGizmoInView = imgui.checkbox("Turn the camera to keep the gizmo handles in view while moving an object", EditorPreferences.KeepGizmoInView)
        EditorPreferences.InvertTabButtonLitColors = imgui.checkbox("Invert tab button active colors", EditorPreferences.InvertTabButtonLitColors)
        EditorPreferences.GitIntegration = imgui.checkbox("Git integration", EditorPreferences.GitIntegration)

        EditorPreferences.ScriptAllowedExecutionTime = imgui.inputnumber("Editor playtest script allowed execution time", EditorPreferences.ScriptAllowedExecutionTime)
        imgui.setitemtooltip("This is independent of the time-out length applied for scripts in exported projects")

        EditorPreferences.CancelPlaytestWhenScriptTimesOut = imgui.checkbox("Cancel playtests when a script times-out", EditorPreferences.CancelPlaytestWhenScriptTimesOut)

        imgui.separator()

        imgui.text(`Your projects directory is:\n{ EditorPreferences.ProjectsDirectory }`)
        imgui.setitemtooltip(EditorPreferences.ProjectsDirectory)

        if imgui.button("Change") then
            local newDir = fs.promptopenfolder("Select Projects directory", EditorPreferences.ProjectsDirectory)

            if newDir and fs.resolvepathabsolute(newDir) ~= fs.resolvepathabsolute(EditorPreferences.ProjectsDirectory) then
                local moveSuccess, err = pcall(function()
                    for p, t in fs.listdir(EditorPreferences.ProjectsDirectory) do
                        local subst, count = p:gsub(fs.resolvepathabsolute(EditorPreferences.ProjectsDirectory), fs.resolvepathabsolute(newDir))

                        if count == 0 then
                            error(`Failed to substitute '{fs.resolvepathabsolute(newDir)}' to replace '{fs.resolvepathabsolute(EditorPreferences.ProjectsDirectory)}' in '{p}'`)
                        end

                        fs.move(p, subst)
                    end
                end)

                if not moveSuccess then
                    Engine:ShowMessageBox(
                        "Failed to move projects",
                        `Failed to move your projects from '{ fs.resolvepathabsolute(EditorPreferences.ProjectsDirectory) }' to '{ fs.resolvepathabsolute(newDir) }'.\n\nError: { err }\n\n`
                        .. `Your Projects directory has not been updated.`,
                        "ok",
                        "error"
                    )
                else
                    fs.remove(EditorPreferences.ProjectsDirectory)
                    EditorPreferences.ProjectsDirectory = newDir
                end
            end
        end

        imgui.urllink("https://youtu.be/YCjNT9qGjh4?t=7193")

        if imgui.treenode("##") then
            imgui.urllink("Configure spyware settings", SPYWARE_SETTINGS_URL)
            imgui.treepop()
        end

        imgui.endpopup()
        wasSettingsOpen = true

    elseif wasSettingsOpen then
        wasSettingsOpen = false
        saveSettings()
    end
end

local function renderOutput()
    local renderOutput = false
    local outputBegan = false
    if EditorPreferences.ToolsOpen.Output then
        renderOutput, EditorPreferences.ToolsOpen.Output = imgui.begin("Output", "h", true)
        outputBegan = true
    end

    if renderOutput then
        local logMessageHoveringWip: number = -1
        local smid = 0
        local rightClickedLogMessage = false

        for mi, message in Log do
            imgui.pushstylecolor(0, { 0.35, 0.35, 0.35, 1 })

            imgui.text(message.Time)

            imgui.popstylecolor()

            local LogColors = {
                [Enum.LogType.None]    = { 1, 1, 1, 1 },
                [Enum.LogType.Info]    = { 1, 1, 1, 1 },
                [Enum.LogType.Warning] = { 255 / 255, 171 / 255, 61 / 255, 1 }, -- warning
                [Enum.LogType.Error]   = { 255 / 255, 61 / 255, 61 / 255, 1 }   -- error
            }

            local logColor = LogColors[message.Type]
            imgui.pushstylecolor(0, logColor)
            imgui.pushstylecolor(52, logColor)

            for smi, subMessage in message.Message do
                smid += 1

                if #subMessage.Content == 0 then
                    continue
                end

                local prev = message.Message[smi - 1]
                if not prev or prev.Newline then
                    imgui.sameline()
                end

                local tooltip = subMessage.Tooltip
                local context = message.Message[1].ExtraTags
                if #context <= 2 or context == "LogContext:Main" then
                    context = ""

                    for _, m in message.Message do
                        if #m.ExtraTags > 2 then
                            context = m.ExtraTags
                            tooltip = m.Tooltip
                            break
                        end
                    end
                end

                local _
                _, tooltip = canInteractWithLogSubMessage(context, subMessage.DirectValue)

                if #tooltip == 0 then
                    for _, m in message.Message do
                        if #m.Tooltip > 0 then
                            tooltip = m.Tooltip
                            break
                        end
                    end
                end

                local canInteract = (message.Source or subMessage.CanInteract) and LogMessageHovering == smid and tooltip ~= "From Command Bar"
                local doInteract = false

                if type(subMessage.DirectValue) == "table" then
                    if not message.OpenTables then
                        message.OpenTables = {}
                    end

                    if not message.TablesWithAnyElements then
                        message.TablesWithAnyElements = {}
                    end

                    local function hasAnyElements(t): boolean
                        local cached = message.TablesWithAnyElements[t]

                        if cached == nil then
                            for _ in t do
                                message.TablesWithAnyElements[t] = true
                                return true
                            end
                            message.TablesWithAnyElements[t] = false
                            return false
                        else
                            return cached
                        end
                    end

                    local startX = imgui.cursorposition().x

                    local function renderTable(t: { [unknown]: unknown }, depth: number, pos: vector?)
                        local id = tostring(t)
                        imgui.pushid(id)

                        local open = message.OpenTables[t]
                        local hasElements = hasAnyElements(t)

                        if message.TablePrettyChangeOpen[id] ~= nil then
                            imgui.setnextitemopen(message.TablePrettyChangeOpen[id])
                            message.TablePrettyChangeOpen[id] = nil
                        end

                        local prev = imgui.cursorposition()
                        local nodeOpen = imgui.treenode("##", "aw | ar | " .. (if depth == 0 then "dlf" else "")  .. (if hasElements then "" else " lf"))
                        local after = imgui.cursorposition()

                        if imgui.itemclicked() then
                            message.TablePrettyChangeOpen[id] = not message.TablePrettyOpen[id]
                        end

                        imgui.dummy(64, 64)
                        imgui.setcursorposition(prev + vector.create(16, 0))
                        imgui.text(if (open and hasElements) then "{" else (if hasElements then "{...}" else "{}"))
                        imgui.setcursorposition(after)

                        message.TablePrettyOpen[id] = nodeOpen

                        if nodeOpen then
                            message.OpenTables[t] = true
                            for k, v in t do
                                imgui.setcursorposition(imgui.cursorposition() + vector.create(startX - 20, 0))

                                local kstr = k
                                if type(k) == "string" then
                                    if not k:match("%W") then
                                        kstr = k
                                    else
                                        kstr = `["{ k }"]`
                                    end
                                else
                                    kstr = `[{ k }]`
                                end

                                if type(v) == "table" then
                                    local tpos = imgui.cursorposition()
                                    if imgui.treenode(kstr :: string .. " = ", "dlf | lf") then
                                        imgui.treepop()
                                    end

                                    if imgui.itemclicked() then
                                        local cid = tostring(v)
                                        message.TablePrettyChangeOpen[cid] = not message.TablePrettyOpen[cid]
                                    end

                                    imgui.sameline()
                                    imgui.setcursorposition(imgui.cursorposition() - vector.create(12, 0))
                                    -- TODO luau cast
                                    renderTable(v :: any, depth + 1, tpos)
                                else
                                    if typeof(k) == "GameObject" then
                                        if imgui.textlink(kstr) then
                                            DeveloperTools:SetExplorerSelections({ k })
                                        end
                                        imgui.sameline()
                                        imgui.text(" = ")
                                    else
                                        if imgui.treenode(kstr :: string .. " = ", "dlf | lf") then
                                            imgui.treepop()
                                        end
                                    end

                                    imgui.sameline()
                                    imgui.setcursorposition(imgui.cursorposition() - vector.create(8, 0))

                                    if typeof(v) == "GameObject" then
                                        if imgui.textlink(tostring(v)) then
                                            -- TODO luau cast
                                            DeveloperTools:SetExplorerSelections({ v :: any })
                                        end
                                    else
                                        local vts = tostring(v) .. ","
                                        if type(v) == "string" then
                                            vts = `"{ v }",`
                                        elseif type(v) ~= "number" and type(v) ~= "boolean" then
                                            vts = `[{ v }],`
                                        end

                                        imgui.text(vts)
                                    end
                                end
                            end

                            imgui.treepop()

                            if hasElements then
                                local p = (if depth > 0 then assert(pos) + vector.create(13, 0) else prev)
                                imgui.setcursorposition(vector.create(assert(p.x) + 8, imgui.cursorposition().y))
                                imgui.text("}")
                            end
                        else
                            message.OpenTables[t] = false
                        end

                        imgui.popid()
                    end

                    imgui.setcursorposition(imgui.cursorposition() - vector.create(8, 0))
                    -- TODO luau cast
                    renderTable(subMessage.DirectValue :: any, 0)
                else
                    if canInteract then
                        doInteract = imgui.textlink(subMessage.Content)
                    else
                        imgui.text(subMessage.Content)
                    end
                end

                if doInteract then
                    interactWithLogSubMessage(context, subMessage.DirectValue)
                end

                if message.Source and message.Line and message.Source ~= "CommandBar" then
                    if imgui.itemclicked(Enum.MouseButton.Right) then
                        DeveloperTools:OpenTextDocument(message.Source, message.Line)
                        rightClickedLogMessage = true
                    end
                end

                if imgui.itemhovered() then
                    logMessageHoveringWip = smid
                end

                if (#tooltip > 0 or #context > 0) and imgui.itemhovered() and imgui.begintooltip() then
                    if #tooltip > 0 then
                        imgui.text(tooltip)
                    end

                    if #context > 0 then
                        if #tooltip > 0 then
                            imgui.newline()
                        end

                        imgui.pushstylecolor(0, { 0.35, 0.35, 0.35, 1 })

                        imgui.text(context)

                        imgui.popstylecolor()
                    end

                    imgui.endtooltip()
                end
            end

            imgui.popstylecolor()
            imgui.popstylecolor()
        end

        if NewLogMessage then
            imgui.scrollherey()
            NewLogMessage = false
        end

        if not rightClickedLogMessage and imgui.windowhovered() and PlayerInput:IsMouseButtonPressed(Enum.MouseButton.Right) then
            imgui.openpopup("OutputContextMenu")
        end

        if imgui.beginpopup("OutputContextMenu") then
            if imgui.menuitem("Clear Output") then
                Log = {}
            end

            imgui.endpopup()
        end

        LogMessageHovering = logMessageHoveringWip
    end

    if outputBegan then
        imgui.endw()
    end
end

function CommonUI.RenderViewport(DeltaTime: number)
    if CurrentStage == "ProjectEditing" and Project.Workspace then
        local mouseVec = workspace:ScreenPointToVector(PlayerInput:GetCursorPosition(), 1000)
        local rayresult = if not PlayerInput.IsMouseSunk
                            then Project.Workspace:Raycast(workspace.SceneCamera.Transform.Position, mouseVec, { SelectionPart })
                            else nil

        if rayresult then
            SelectionPart.Transform = rayresult.Object.Transform
            SelectionPart.Size = rayresult.Object.Size + vector.create(0.05, 0.05, 0.05)
            SelectionPart.Parent = workspace

            if PlayerInput:IsMouseButtonPressed(Enum.MouseButton.Left) and isMouseWithinViewport() then
                if PlayerInput:IsKeyPressed(Enum.Key.LeftControl) then -- left control
                    local sels = DeveloperTools:GetExplorerSelections()

                    if not table.find(sels, rayresult.Object) then
                        table.insert(sels, rayresult.Object :: GameObject)
                    end

                    DeveloperTools:SetExplorerSelections(sels)
                else
                    DeveloperTools:SetExplorerSelections({ rayresult.Object })
                end
            end
        else
            SelectionPart.Parent = nil
        end
    else
        for _, v in PrevSelections do
            v:Destroy()
        end
        PrevSelections = {}
    end

    local newSelections = DeveloperTools:GetExplorerSelections()
    local selmap: { [number]: GameObject } = {}
    for i, sel_ in newSelections do
        if not sel_:HasComponent("Transform") then
            continue
        end

        local sel = (sel_ :: any) :: GameObject & EcTransform
        selmap[sel.ObjectId] = sel

        if not PrevSelections[sel.ObjectId] then
            local new = SelectionPart:Duplicate()
            -- TODO luau report bug cast
            new = (new :: any)
            new.Transform, new.Size = sel.Transform, sel.Size + vector.create(0.1, 0.1, 0.1)
            new.Tint = Color.new(84/255, 150/255, 1)
            new.Parent = workspace
            PrevSelections[sel.ObjectId] = new
        else
            local box = PrevSelections[sel.ObjectId]
            box.Transform, box.Size = sel.Transform, sel.Size
        end
    end

    for id, box in PrevSelections do
        local sel = selmap[id]
        if not sel or not sel:HasComponent("Transform") then
            box:Destroy()
            PrevSelections[id] = nil
        end
    end

    PluginManager.OnUpdate(DeltaTime, CurrentStage)
    CoreTools.Update(Project, EditorPreferences, ViewportRenderPosition, ViewportRenderSize, DeltaTime)
    HistoryView.Render()
    TagViewer.Render()

    if EditorPreferences.ToolsOpen.BottomBar then
        local w = Renderer.Resolution

        imgui.setnextwindowposition(0, w.y - 35)
        imgui.setnextwindowsize(w.x, 35)

        imgui.begin("Bottom Bar", "nt | nd | nr | ns")

        imgui.setcursorposition(imgui.cursorposition() + vector.create(0, 2))
        imgui.text("> ")
        imgui.sameline()
        imgui.setcursorposition(imgui.cursorposition() - vector.create(0, 2))

        CommandBarCommand = imgui.inputstring(if CommandBarBlinkFrame == 1 then "##CommandBarBlinking" else "##Command", CommandBarCommand)
        CommandBarActive = imgui.itemactive()

        if CommandBarBlinkFrame == 1 then
            CommandBarBlinkFrame += 1

        elseif CommandBarBlinkFrame == 2 then
            CommandBarBlinkFrame = 0
            imgui.focuskeyboardhere(-1)
        end

        if CommandBarPostRun then
            imgui.focuskeyboardhere(-1)
            CommandBarPostRun = false
        end

        imgui.sameline()

        local noteStr = `{ Engine.Framerate } FPS ({ CurrentStage }) | { Project.Name }`
        imgui.text(noteStr)
        imgui.setitemtooltip(noteStr)

        imgui.endw()
    end

    renderOutput()

    local canInteract = IsHoveringViewportTitlebar --or (not IsHoveringViewport and imgui.windowhovered("a | b"))
    local render, open = imgui.begin(Project.Name .. "###Viewport", if canInteract then "nb" else "ni | nb", true)

    if not open then
        task.defer(changeStageTo, "SelectProject" :: "SelectProject")
    end

    if not render then
        PlayerInput:SetViewportInputRect(vector.zero, vector.create(1, 1))
        imgui.endw()
        return
    end

    local cpos = PlayerInput:GetCursorPosition()
    local w = imgui.windowposition()
    local ws = imgui.windowsize()

    local ews = Renderer.Resolution
    ViewportRenderPosition = vector.create(w.x, w.y)
    ViewportRenderSize = vector.create(ws.x, ws.y)

    imgui.setviewportdockspace(0, 19, ews.x, ews.y - (if EditorPreferences.ToolsOpen.BottomBar then 54 else 19))
    PlayerInput:SetViewportInputRect(ViewportRenderPosition, ViewportRenderSize)

    ViewportLeft = w.x - 15
    ViewportRight = w.x + ws.x + 15
    ViewportTop = w.y
    ViewportBottom = w.y + ws.y

    if cpos.x >= w.x - 15 and cpos.x <= w.x + ws.x + 15 then
        if cpos.y >= w.y and cpos.y < w.y + 16 then
            IsHoveringViewportTitlebar = true
        else
            IsHoveringViewportTitlebar = false
        end
    else
        IsHoveringViewportTitlebar = false
    end
    isMouseWithinViewport()

    imgui.setcursorposition(0, ws.y - ews.y)

    imgui.image("!Framebuffer:Main", nil, true)

    imgui.endw()
end

return CommonUI
