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
    ReimportableModelTag = "Phoenix_ReimportableModel",
    EditorBuild = 9,
    StdPath = "@_editres/scripts/std",
    GitHubUrl = "https://github.com/PhoenixWhitefire/PhoenixEngine",
    GameEditingVMName = "GameEditingVM",
    PlaytestVMName = "Playtest",
    FallbackImage = "@_etextures/editor-icons/fallback.png",

    GetLogo = getLogo
})
