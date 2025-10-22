--!strict
local FALLBACK_IMAGE = "textures/editor-icons/fallback.png"

local Types = require("./Types.luau")
type EditorStage = Types.Stage
type Plugin = Types.Plugin
type ToolbarButton = Types.ToolbarButton

local PluginManager = {}
PluginManager.Plugins = {} :: { Plugin }
local IsPluginManagementWindowOpen = false
local PluginSettings: { [string]: { Enabled: boolean } } = {}

local function getPluginSettings(plugin: Plugin): index<typeof(PluginSettings), string>
    local settings = PluginSettings[plugin.Path]
    if not settings then
        settings = { Enabled = true }
        PluginSettings[plugin.Path] = settings
    end

    return settings
end

local function renderPluginManagement()
    if not imgui.begin("Plugin Manager") then
        imgui.endw()
        return
    end

    local pluginSettingsChanged = false

    for _, plugin in PluginManager.Plugins do
        imgui.pushid(plugin.Path)
        local settings = getPluginSettings(plugin)

        local cx, cy = imgui.cursorposition()

        imgui.image(plugin.Icon or FALLBACK_IMAGE, { 150, 150 }, false, if settings.Enabled then { 1, 1, 1, 1 } else { 0.5, 0.5, 0.5, 1. })

        imgui.setcursorposition(cx + 175, cy)

        local wasEnabled = settings.Enabled
        settings.Enabled = imgui.checkbox("##", wasEnabled)
        imgui.setitemtooltip("Enabled")
        imgui.sameline()

        imgui.text(plugin.Name)

        imgui.setcursorposition(cx + 175, cy + 20)
        imgui.text(plugin.Description or "")

        imgui.setcursorposition(cx + 175, cy + 140)
        imgui.text(plugin.Path)

        imgui.setcursorposition(cx, cy + 160)
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
        fs.write("./plugin-settings.json", json.encode(PluginSettings))
    end
end

local function loadPluginsRecursive(ParentPath)
    for path, type in fs.listdir(ParentPath) do
        if type == "f" then
            if path:sub(#path - 4, #path) == ".luau" then
                local co, err = loadthreadfromfile(path)

                if co then
                    local success, data: Plugin = coroutine.resume(co)

                    if success then
                        data.Path = path:sub(assert((path:find("/plugins"))) + 1, #path)
                        table.freeze(data)
                        table.insert(PluginManager.Plugins, data)

                        if getPluginSettings(data).Enabled and data.OnLoad then
                            data:OnLoad()
                        end
                    else
                        print(`Failed to load-execute plugin {path}: {data}`)
                    end
                else
                    print(`Failed to load plugin {path}: {err}`)
                end
            end
        else
            loadPluginsRecursive(path)
        end
    end
end

function PluginManager.Load()
    if not fs.isdirectory("./plugins") then
        return
    end

    if fs.isfile("./plugin-settings.json") then
        PluginSettings = json.parse(fs.read("./plugin-settings.json") or "") or {}
    end

    loadPluginsRecursive("./plugins")
end

function PluginManager.Shutdown()
    for _, plugin in PluginManager.Plugins do
        if plugin.OnShutdown then
            plugin:OnShutdown()
        end
    end

    table.clear(PluginManager.Plugins)
end

function PluginManager.OnEditorStageChanged(OldStage: EditorStage, NewStage: EditorStage)
    for _, plugin in PluginManager.Plugins do
        if getPluginSettings(plugin).Enabled and plugin.OnEditorStageChanged and table.find(plugin.ActiveStages, NewStage) then
            plugin:OnEditorStageChanged(OldStage, NewStage)
        end
    end
end

function PluginManager.OnUpdate(DeltaTime: number, CurrentStage: EditorStage)
    for _, plugin in PluginManager.Plugins do
        if getPluginSettings(plugin).Enabled and plugin.OnUpdate and table.find(plugin.ActiveStages, CurrentStage) then
            plugin:OnUpdate(DeltaTime)
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
        Enabled = true,
        Lit = IsPluginManagementWindowOpen,
        OnClick = togglePluginManagement
    })
    table.insert(buttons, separator())

    for _, plugin in PluginManager.Plugins do
        if getPluginSettings(plugin).Enabled and table.find(plugin.ActiveStages, CurrentStage) then
            for _, t in plugin.ToolbarButtons do
                table.insert(buttons, t)
            end
        end

        table.insert(buttons, separator())
    end

    table.remove(buttons, #buttons)

    return buttons
end

return PluginManager
