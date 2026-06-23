-- stages/SelectProject.luau, 29/05/2026
local SelectProject = {}

local DeveloperTools = game:GetService("DeveloperTools")
local AssetManager = game:GetService("AssetManager")
local Renderer = game:GetService("Renderer")
local Engine = game:GetService("Engine")

local PluginManager = require("@ed/PluginManager")
local Configuration = require("@ed/Configuration")
local Preferences = require("@ed/Preferences")
local CommonUI = require("@ed/CommonUI")
local Output = require("@ed/Output")
local State = require("@ed/State")
local Theme = require("@ed/Theme")

function SelectProject.Enter(_prev)
    if not fs.isfile(Configuration.RecentProjectsPath) then
        fs.write(Configuration.RecentProjectsPath, "[]")
    end

    local rpstr, err = fs.read(Configuration.RecentProjectsPath)

    if not rpstr then
        print(`Couldn't read { Configuration.RecentProjectsPath }: { err }`)
        return
    end

    local rpjson = json.parse(rpstr)
    assert(typeof(rpjson) == "table")

    State.RecentProjects = rpjson :: typeof(State.RecentProjects)
    State.SavedProjectsCache = nil

    PluginManager.Shutdown()
    fs.setunqualifiedroot("./")

    for _, tool in DeveloperTools:GetToolNames() do
        DeveloperTools:SetToolEnabled(tool, false)
    end
end

function SelectProject.Leave(_new)
    PluginManager.Load()

    if Preferences.ClearOutputOnProjectOpen then
        table.clear(Output.Log)
    end
end

local SelectorHoveringOver: number? = nil
local OpenProjectContextMenu = false
local ProjectContextMenuTarget = 0

local function renderProjectSelectorTiles(Projects: { string } | { State.CachedProjectInfo })
    local cpos = imgui.cursorposition()
    local startX = cpos.x

    local windowSizeX = Renderer.Resolution.x
    local lastnl = #Projects
    local hoveringOverWip = nil

    for i = #Projects, 1, -1 do
        local projInfo = Projects[i]
        local projName = if typeof(projInfo) == "string" then projInfo else projInfo.Name
        local clicked = false
        local hovered = false
        cpos = imgui.cursorposition()

        imgui.dummy(300, 300)
        imgui.setcursorposition(cpos.x, cpos.y)

        if SelectorHoveringOver == i then
            imgui.pushstylecolor(Enum.ImGuiColor.Border, Theme.TabButtonHoveringBorder)

            imgui.setcursorposition(cpos.x - 10, cpos.y - 10)
            imgui.beginchild(projName .. "_Selection", vector.create(270, 280), "b", "ni")
            imgui.endchild()
            imgui.setcursorposition(cpos.x, cpos.y)

            imgui.popstylecolor()
        end

        if imgui.itemclicked(Enum.MouseButton.Right) then
            OpenProjectContextMenu = true
            ProjectContextMenuTarget = i
        end

        imgui.image(`{ Preferences.ProjectsDirectory }/{ projName }/icon.png`, vector.create(250, 250))
        clicked, hovered = imgui.itemclicked(), imgui.itemhovered()
        imgui.setcursorposition(cpos.x, cpos.y + 250)
        imgui.text(projName)
        clicked, hovered = clicked or imgui.itemclicked(), hovered or imgui.itemhovered()

        if clicked then
            task.defer(State.OpenProject, State, projName)
        end

        if hovered then
            hoveringOverWip = i

            if typeof(projInfo) ~= "string" then
                imgui.settooltip(`Created: { os.date("%Y.%m.%d", projInfo.Created) }\nSaved: { os.date("%Y.%m.%d", projInfo.Saved) }\nEngine: { projInfo.EngineVersion }`)
            end
        end

        if (lastnl - i) * 350 + 170 + 250 + 75 + startX > windowSizeX then
            -- new row
            cpos = vector.create(imgui.cursorposition().x, cpos.y + 270)
            imgui.setcursorposition(startX, cpos.y + 10)
            lastnl = i - 1
        else
            -- same row
            imgui.setcursorposition(cpos.x + 275 - 15, cpos.y)
            imgui.dummy(0, 0)
            imgui.sameline()
        end
    end

    SelectorHoveringOver = hoveringOverWip
end

local function promptDeleteProject(Name)
    local buttonSelection = Engine:ShowMessageBox(
        "Are you sure?",
        `Are you sure you want to delete '{ Name }'? This cannot be undone.`,
        "yesno",
        "warning"
    )

    if buttonSelection == 1 then
        fs.remove(`{ Preferences.ProjectsDirectory }/{ Name }`)

        AssetManager:UnloadTexture(`{ Preferences.ProjectsDirectory }/{ Name }/icon.png`)
        table.remove(State.RecentProjects, table.find(State.RecentProjects, Name))
        State:SaveRecents()

        State.SavedProjectsCache = nil
    end
end

local function renderRenderProjects()
    imgui.text("Recent")
    imgui.newline()

    local cpos = imgui.cursorposition()
    imgui.setcursorposition(cpos.x, cpos.y + 10)

    cpos = imgui.cursorposition()
    imgui.setcursorposition(cpos.x + 10, cpos.y)

    renderProjectSelectorTiles(State.RecentProjects)

    if OpenProjectContextMenu then
        imgui.openpopup("ProjectContextMenu")
        OpenProjectContextMenu = false
    end

    if imgui.beginpopup("ProjectContextMenu") then
        if imgui.menuitem("Remove from Recents") then
            table.remove(State.RecentProjects, ProjectContextMenuTarget)
            -- TODO: fix cast
            fs.write(Configuration.RecentProjectsPath, json.encode(State.RecentProjects :: JsonSerializable))
        end

        if imgui.menuitem("Delete") then
            promptDeleteProject(State.RecentProjects[ProjectContextMenuTarget])
        end

        imgui.endpopup()
    end
end

local function renderSavedProjects()
    if not State.SavedProjectsCache then
        State.SavedProjectsCache = {}
        assert(State.SavedProjectsCache)

        for path, type in fs.listdir(Preferences.ProjectsDirectory) do
            if type == "d" and fs.isfile(path .. "/phoenix.conf") then
                local conf = json.parse(assert(fs.read(path .. "/phoenix.conf")))
                assert(typeof(conf) == "table")
                local title = conf["GameTitle" :: string]
                assert(typeof(title == "string"))

                local engine = "Unknown"
                local savedTime = 0
                local created = 0

                local editorConfContents = fs.read(path .. "/phoenix.editor")

                if editorConfContents then
                    local editorConf = json.parse(assert(fs.read(path .. "/phoenix.editor"))) :: any
                    local savedInfo = editorConf.Saved
                    local createdInfo = conf["Created" :: string] :: any

                    engine = if typeof(savedInfo) == "table" then savedInfo.EngineVersion else "0"
                    savedTime = if typeof(savedInfo) == "table" then savedInfo.Time else 0
                    created = if typeof(createdInfo) == "table" then createdInfo.Time else 0
                end

                table.insert(State.SavedProjectsCache, {
                    EngineVersion = engine,
                    Created = created,
                    Saved = savedTime,
                    Name = title
                })
            end
        end
    end

    imgui.text("All Projects")
    imgui.newline()

    local cpos = imgui.cursorposition()
    imgui.setcursorposition(cpos.x, cpos.y + 10)

    cpos = imgui.cursorposition()
    imgui.setcursorposition(cpos.x + 10, cpos.y)

    renderProjectSelectorTiles(assert(State.SavedProjectsCache))

    if OpenProjectContextMenu then
        imgui.openpopup("ProjectContextMenu")
        OpenProjectContextMenu = false
    end
    
    if imgui.beginpopup("ProjectContextMenu") then
        if State.SavedProjectsCache and imgui.menuitem("Delete") then
            local proj = State.SavedProjectsCache[ProjectContextMenuTarget]
            promptDeleteProject(proj.Name)
        end

        imgui.endpopup()
    end
end

local ProjectCategories = {
    {
        Name = "Recent",
        Icon = "@etextures/editor-icons/recent.png",
        Render = renderRenderProjects,
        OnSwitchTo = nil,
    },
    {
        Name = "Saved",
        Icon = "@etextures/editor-icons/open.png",
        Render = renderSavedProjects,
        OnSwitchTo = function()
            State.SavedProjectsCache = nil
        end,
    }
}

local SelectedProjectsCategory = 1
local PrevSelectedProjectsCategory = 0

function SelectProject.Render(DeltaTime: number)
    assert(typeof(DeltaTime) == "number")

    if imgui.beginfullscreen("Start Page", 0, 18) then
        local startPos = imgui.cursorposition()

        if imgui.beginchild("SidePanel", vector.create(165, 0), "b") then
            local cpos = imgui.cursorposition()
            imgui.setcursorposition(cpos.x - 4, cpos.y)

            if imgui.imagebutton("+", "@etextures/editor-icons/new.png", vector.create(150, 150)) then
                task.defer(State.CreateNewProject, State, nil)
            end
            imgui.setitemtooltip("Create new Project")

            cpos = imgui.cursorposition()

            for i, cat in ProjectCategories do
                if SelectedProjectsCategory == i then
                    imgui.setcursorposition(cpos.x - 5, cpos.y - 2)
                    imgui.pushstylecolor(Enum.ImGuiColor.Border, Theme.TabButtonHoveringBorder)

                    imgui.beginchild("Selection", vector.create(160, 160), "b", "ni")
                    imgui.endchild()

                    imgui.popstylecolor()
                end

                imgui.setcursorposition(cpos.x - 4, cpos.y)

                if imgui.imagebutton(cat.Name, cat.Icon, vector.create(150, 150)) then
                    SelectedProjectsCategory = i
                end
                imgui.setitemtooltip(cat.Name)

                cpos = imgui.cursorposition()
            end

            if PrevSelectedProjectsCategory ~= SelectedProjectsCategory then
                local onSwitchTo = ProjectCategories[SelectedProjectsCategory].OnSwitchTo
                if onSwitchTo then
                    onSwitchTo()
                end

                PrevSelectedProjectsCategory = SelectedProjectsCategory
            end
        end
        imgui.endchild()

        imgui.setcursorposition(startPos.x + 170, startPos.y)

        imgui.beginchild("MainSection", vector.create(0, 0), "b")

        ProjectCategories[SelectedProjectsCategory].Render(startPos.x)

        imgui.dummy()
        imgui.endchild()
    end
    imgui.endw()

    CommonUI.RenderMainMenuBar(DeltaTime)
end

return SelectProject
