-- DefaultThemes.luau, 06/06/2026
local DefaultThemes = {
    Dark = {
        ImGui = {
            Base = "Dark",
            Override = {}
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
    },
    Light = {
        ImGui = {
            Base = "Light",
            Override = {}
        },
    }
}

return DefaultThemes
