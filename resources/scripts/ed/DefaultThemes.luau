-- DefaultThemes.luau, 06/06/2026
type ThemeData = {
    [string]: { number },
    ImGui: {
        Base: string,
        Overrides: {
            [string]: { number }
        }
    },
    Inherit: string?
}

local DefaultThemes: { [string]: ThemeData } = {
    Dark = {
        ImGui = {
            Base = "Dark",
            Overrides = {}
        },

        TabButtonEnabledTint = { 1, 1, 1, 1 },
        TabButtonDisabledTint = { 0.5, 0.5, 0.5, 1 },
        TabButtonLit = { 1, 1, 1, 0.05 },
        TabButtonUnlit = { 1, 1, 1, 0 },
        TabButtonHoveringBorder = { 1, 1, 1, 1 },

        OutputTimestamp = { 0.35, 0.35, 0.35, 1 },
        OutputLogNone = { 1, 1, 1, 1 },
        OutputLogInfo = { 1, 1, 1, 1 },
        OutputLogWarning = { 255 / 255, 171 / 255, 61 / 255, 1 },
        OutputLogError = { 255 / 255, 61 / 255, 61 / 255, 1 },
        OutputContextTooltip = { 0.35, 0.35, 0.35, 1 },

        PlaytestMenubarColor = { 0, 56 / 270, 15 / 270, 1 },
    },
    Light = {
        ImGui = {
            Base = "Light",
            Overrides = {}
        },
        Inherit = "Dark",

        TabButtonLit = { 1, 1, 1, 0.05 },
        TabButtonUnlit = { .1, .1, .1, 0.05 },
        TabButtonHoveringBorder = { 0.2, 0.2, 0.2, 1 },

        OutputLogNone = { 0, 0, 0, 1 },
        OutputLogInfo = { 0, 0, 0, 1 },
        OutputLogWarning = { 255 / 255, 143 / 255, 0 / 255, 1 },
        OutputLogError = { 243 / 255, 0 / 255, 0 / 255, 1 },

        PlaytestMenubarColor = { 118/255, 255 / 255, 154 / 255, 1 },
    }
}

return DefaultThemes
