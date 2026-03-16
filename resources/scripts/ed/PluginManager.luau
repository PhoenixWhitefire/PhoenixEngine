--!strict
local FALLBACK_IMAGE = "textures/editor-icons/fallback.png"

local Types = require("./Types.luau")
type EditorStage = Types.Stage
type Plugin = Types.Plugin
type ToolbarButton = Types.ToolbarButton

local PluginManager = {}
PluginManager.Plugins = {} :: { Plugin }
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
    local render, open = imgui.begin("Plugin Manager", "", true)
    if not render or not open then
        imgui.endw()
        IsPluginManagementWindowOpen = open
        return
    end

    local pluginSettingsChanged = false

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
        fs.write(PLUGIN_SETTINGS_PATH, json.encode(PluginSettings))
    end
end

local function setPluginAlias(ParentPath: string?)
    if ParentPath then
        fs.definealias("plugin", ParentPath)
    end
end

local function loadPlugin(Path, ParentPath: string?)
    local co, err = task.loadfile(Path)

    if co then
        local success, data: Plugin = coroutine.resume(co)

        if success then
            data.Path = Path:sub(assert((Path:find("/plugins"))) + 1, #Path)
            data.ParentPath = ParentPath
            --table.freeze(data)

            if typeof(data.Icon) == "string" then
                setPluginAlias(ParentPath)
                data.Icon = fs.resolvepath(data.Icon)
                fs.removealias("plugin")
            end

            table.insert(PluginManager.Plugins, data)

            if getPluginSettings(data).Enabled and data.OnLoad then
                data:OnLoad()
            end
        else
            warn(`PluginManager: Failed to load-execute plugin { Path }: { data }`)
        end
    else
        warn(`PluginManager: Failed to load plugin { Path }: { err }`)
    end
end

local function loadPlugins(ParentPath)
    for path, type in fs.listdir(ParentPath) do
        if type == "f" then
            if path:sub(#path - 4, #path) == ".luau" then
                loadPlugin(path)
            end
        else
            local mainScriptPath = path .. "/Main.luau"

            if fs.isfile(mainScriptPath) then
                loadPlugin(mainScriptPath, path)
            else
                warn(`PluginManager: Expected { mainScriptPath } to be valid`)
            end
        end
    end
end

function PluginManager.Load()
    if not fs.isdirectory("./plugins") then
        return
    end

    if fs.isfile(PLUGIN_SETTINGS_PATH) then
        local settings = json.parse(fs.read(PLUGIN_SETTINGS_PATH) or "")

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
        if plugin.OnShutdown then
            setPluginAlias(plugin.ParentPath)
            plugin:OnShutdown()
            fs.removealias("plugin")
        end
    end

    table.clear(PluginManager.Plugins)
end

function PluginManager.OnEditorStageChanged(OldStage: EditorStage, NewStage: EditorStage)
    for _, plugin in PluginManager.Plugins do
        if getPluginSettings(plugin).Enabled and plugin.OnEditorStageChanged and table.find(plugin.ActiveStages, NewStage) then
            setPluginAlias(plugin.ParentPath)
            plugin:OnEditorStageChanged(OldStage, NewStage)
            fs.removealias("plugin")
        end
    end
end

function PluginManager.OnUpdate(DeltaTime: number, CurrentStage: EditorStage)
    for _, plugin in PluginManager.Plugins do
        if getPluginSettings(plugin).Enabled and plugin.OnUpdate and table.find(plugin.ActiveStages, CurrentStage) then
            setPluginAlias(plugin.ParentPath)
            plugin:OnUpdate(DeltaTime)
            fs.removealias("plugin")
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
        Icon = "@etextures/editor-icons/plugin-manager.png",
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
                    t.Icon = fs.resolvepath(t.Icon)
                    fs.removealias("plugin")
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
