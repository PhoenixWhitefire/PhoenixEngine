--!strict
local FALLBACK_IMAGE = "textures/editor-icons/fallback.png"

local State = require("./State.luau")
local Types = require("./Types.luau")
type EditorStage = Types.Stage
type Plugin = Types.Plugin
type PluginInternal = Plugin & { _thread: thread }
type ToolbarButton = Types.ToolbarButton

-- local json = require("@std/json")
local fs = require("@std/fs")

local imgui = phoenix.imgui
local json = phoenix.json

local PluginManager = {}
PluginManager.Plugins = {} :: { PluginInternal }
local IsPluginManagementWindowOpen = false
local PluginSettings: { [string]: { Enabled: boolean }? } = {}

local PLUGIN_SETTINGS_PATH = "./editor/plugin-settings.json"

local function getPluginSettings(plugin: Plugin)
    local settings = PluginSettings[plugin.Path]
    if not settings then
        settings = { Enabled = true }
        PluginSettings[plugin.Path] = settings
    end

    return assert(settings)
end

local function renderPluginManagement()
    imgui.setnextwindowsize(0, 250)

    local render, open = imgui.begin("Plugin Manager", "", true)
    if not render or not open then
        imgui.endw()
        IsPluginManagementWindowOpen = open
        return
    end

    local pluginSettingsChanged = false

    if next(PluginManager.Plugins) == nil then
        imgui.text("You don't have any plugins installed\n:)")
    end

    for _, plugin in PluginManager.Plugins do
        imgui.pushid(plugin.Path)
        local settings = getPluginSettings(plugin)

        local cpos = imgui.cursorposition()

        imgui.image(plugin.Icon or FALLBACK_IMAGE, vector.create(150, 150), false, if settings.Enabled then { 1, 1, 1, 1 } else { 0.5, 0.5, 0.5, 1. })

        imgui.setcursorposition(cpos.x + 175, cpos.y)

        local wasEnabled = settings.Enabled
        settings.Enabled = imgui.checkbox("##", wasEnabled)
        imgui.setitemtooltip("Enabled")
        imgui.sameline()

        imgui.text(plugin.Name)

        imgui.setcursorposition(cpos.x + 175, cpos.y + 20)
        imgui.text(plugin.Description or "")

        imgui.setcursorposition(cpos.x + 175, cpos.y + 140)
        imgui.text(plugin.Path)

        imgui.setcursorposition(cpos.x, cpos.y + 160)
        imgui.separator()

        imgui.popid()

        if settings.Enabled ~= wasEnabled then
            pluginSettingsChanged = true

            if not settings.Enabled and plugin.OnShutdown then
                plugin:OnShutdown()

            elseif settings.Enabled and plugin.OnLoad then
                plugin:OnLoad()
            end
        end
    end

    imgui.endw()

    if pluginSettingsChanged then
        fs.writeStringToFile(PLUGIN_SETTINGS_PATH, json.encode(PluginSettings))
    end
end

local function setPluginAlias(ParentPath: string?)
    if ParentPath then
        phoenix.fs.definealias("plugin", ParentPath)
    end
end

local function loadPlugin_(Path, ParentPath: string?)
    local co, err = phoenix.task.loadfile(Path)

    if co then
        phoenix.task.setglobal(co, "plugin", {
            ProjectDatamodel = State.Project.DataModel,
            ProjectWorkspace = State.Project.Workspace,
        })
        setPluginAlias(ParentPath)

        local success, data = coroutine.resume(co)

        if success and data then
            data.Path = Path:sub(assert((Path:find("/plugins"))) + 1, #Path)
            data.ParentPath = ParentPath
            --table.freeze(data)

            if typeof(data.Icon) == "string" then
                data.Icon = phoenix.fs.resolvepath(data.Icon)
            end

            data._thread = co
            table.insert(PluginManager.Plugins, data)

            if getPluginSettings(data).Enabled and data.OnLoad then
                data:OnLoad()
            end
        else
            warn(`PluginManager: Failed to execute plugin '{ Path }': { data }`)
        end

        phoenix.fs.removealias("plugin")
    else
        warn(`PluginManager: Failed to load plugin '{ Path }': { err }`)
    end
end

local function loadPlugin(Path, ParentPath: string?)
    local success, err = pcall(function()
        loadPlugin_(Path, ParentPath)
    end)

    if not success then
        warn(`PluginManager: Error while loading plugin '{ Path }': { err }`)
    end
end

local function loadPlugins(ParentPath: string)
    for _, entry in fs.listDirectory(ParentPath) do
        if entry.type == "file" then
            if entry.name:sub(#entry.name - 4, #entry.name) == ".luau" then
                loadPlugin(entry.name)
            end
        else
            local path = ParentPath .. "/" .. entry.name
            local mainScriptPath = path .. "/Main.luau"

            if fs.type(mainScriptPath) == "file" then
                loadPlugin(mainScriptPath, path)
            else
                warn(`PluginManager: Expected { mainScriptPath } to be valid`)
            end
        end
    end
end

function PluginManager.Load()
    if fs.type("./plugins") ~= "dir" then
        return
    end

    if fs.type(PLUGIN_SETTINGS_PATH) == "file" then
        local settings = json.parse(fs.readFileToString(PLUGIN_SETTINGS_PATH) or "")

        if typeof(settings) == "table" then
            PluginSettings = settings :: { [string]: { Enabled: boolean }? }
        else
            warn(`PluginManager: Settings file corrupt: '{ settings }' ({ typeof(settings) })`)
        end
    end

    loadPlugins("./plugins")
end

function PluginManager.Shutdown()
    for _, plugin in PluginManager.Plugins do
        if getPluginSettings(plugin).Enabled and plugin.OnShutdown then
            setPluginAlias(plugin.ParentPath)
            plugin:OnShutdown()
            phoenix.fs.removealias("plugin")
            coroutine.close(plugin._thread)
        end
    end

    table.clear(PluginManager.Plugins)
end

function PluginManager.OnEditorStageChanged(OldStage: EditorStage, NewStage: EditorStage)
    for _, plugin in PluginManager.Plugins do
        if getPluginSettings(plugin).Enabled and plugin.OnEditorStageChanged and table.find(plugin.ActiveStages, NewStage) then
            setPluginAlias(plugin.ParentPath)
            plugin:OnEditorStageChanged(OldStage, NewStage)
            phoenix.fs.removealias("plugin")
        end
    end
end

function PluginManager.OnUpdate(DeltaTime: number, CurrentStage: EditorStage)
    for _, plugin in PluginManager.Plugins do
        if getPluginSettings(plugin).Enabled and plugin.OnUpdate and table.find(plugin.ActiveStages, CurrentStage) then
            debug.zonebegin(plugin.Name)

            setPluginAlias(plugin.ParentPath)
            local s, e = pcall(plugin.OnUpdate, plugin, DeltaTime)
            phoenix.fs.removealias("plugin")

            if not s then
                warn(`Plugin '{ plugin.Name }' errored on update: { e }`)
            end

            debug.zoneend()
        end
    end

    if IsPluginManagementWindowOpen then
        renderPluginManagement()
    end
end

local function togglePluginManagement()
    IsPluginManagementWindowOpen = not IsPluginManagementWindowOpen
end

local function separator() : ToolbarButton
    return {
        Name = "_pluginSeparator",
        Text = "_pluginSeparator",
        Enabled = true,
        IsSeparator = true,
        OnClick = function() end
    }
end

function PluginManager.GetToolbarButtons(CurrentStage: EditorStage): { ToolbarButton }
    local buttons = {}
    table.insert(buttons, {
        Text = "Manage Plugins",
        Icon = "@_etextures/editor-icons/plugin-manager.png",
        Enabled = true,
        Lit = IsPluginManagementWindowOpen,
        OnClick = togglePluginManagement
    })
    table.insert(buttons, separator())

    for _, plugin in PluginManager.Plugins do
        if getPluginSettings(plugin).Enabled and table.find(plugin.ActiveStages, CurrentStage) then
            for _, t in plugin.ToolbarButtons do
                if t.Icon and t.Icon:find("@") then
                    setPluginAlias(plugin.ParentPath)
                    t.Icon = phoenix.fs.resolvepath(t.Icon)
                    phoenix.fs.removealias("plugin")
                end

                table.insert(buttons, t)
            end
        end

        table.insert(buttons, separator())
    end

    table.remove(buttons, #buttons)

    return buttons
end

return PluginManager
