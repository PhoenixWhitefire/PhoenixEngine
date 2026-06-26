-- Configuration.luau, 29/05/2026
local Logos = {
    "aroace",
    "bi",
    "gay",
    "lesbian",
    "pride",
    "trans",
}

local CachedLogos = {}

local function getLogo(Target: string?)
    if Target and CachedLogos[Target] then
        return CachedLogos[Target]
    else
        local logo = `@_etextures/logos/{ Logos[math.random(1, #Logos)] }.png`

        if Target then
            CachedLogos[Target] = logo
        end

        return logo
    end
end

return table.freeze({
    RecentProjectsPath = "./editor/recent-projects.json",
    ReimportableModelTag = "Ed_ReimportableModel",
    EditorBuild = 5,
    EditorScriptsAllowedExecutionTime = 30,
    GitHubUrl = "https://github.com/PhoenixWhitefire/PhoenixEngine",
    GameEditingVMName = "GameEditingVM",
    PlaytestVMName = "Playtest",
    FallbackImage = "@_etextures/editor-icons/fallback.png",

    CoreAssets = {
        "shaders/error.shp",
        "shaders/error.frag",
        "shaders/ui.shp",
        "shaders/ui.vert",
        "shaders/ui.frag",
        "shaders/worldUber.shp",
        "shaders/worldUber.vert",
        "shaders/worldUber.frag",
        "shaders/worldUber.geom",
        "shaders/worldUberSkinned.shp",
        "shaders/worldUberSkinned.vert",
        "shaders/worldUberTriProjected.shp",
        "shaders/skybox.shp",
        "shaders/skybox.vert",
        "shaders/skybox.frag",
        "shaders/particle.shp",
        "shaders/particle.vert",
        "shaders/particle.frag",
        "shaders/postprocessing.shp",
        "shaders/postprocessing.vert",
        "shaders/postprocessing.frag",
        "shaders/boxframe.shp",
        "shaders/boxframe.frag",
        "shaders/include/worldCommon.frag",
    },

    GetLogo = getLogo
})
