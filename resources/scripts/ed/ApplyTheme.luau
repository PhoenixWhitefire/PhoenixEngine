-- ApplyTheme.luau, 06/06/2026
type ThemeData = typeof(require("@ed/Theme"))

local ErrorMessage = require("@ed/ErrorMessage")
local GetThemeData = require("@ed/GetThemeData")
local Theme = require("@ed/Theme")

local BaseImGuiStyles = {
    Dark = "d",
    Light = "l",
    Classic = "c"
}

local applyThemeByName: (Name: string) -> ()
local themeLineage = {}

local function applyTheme(themeData: ThemeData, current: string)
    if themeData.Inherit and themeData.Inherit ~= current then
        applyThemeByName(themeData.Inherit)
    end

    imgui.stylecolors(BaseImGuiStyles[themeData.ImGui.Base])

    for name, value in themeData.ImGui.Overrides do
        imgui.setstylecolor((Enum.ImGuiColor :: any)[name], value)
    end

    for key, value in themeData do
        if key == "ImGui" then
            continue
        end

        Theme[key] = value
    end
end

function applyThemeByName(Name: string)
    if table.find(themeLineage, Name) then
        local lineage = ""

        for _, theme in themeLineage do
            lineage ..= `{ theme } -> `
        end
        lineage:sub(1, #lineage - 3)

        ErrorMessage.Push(`{ Name } relies on itself! { lineage }`)
        return
    end

    table.insert(themeLineage, Name)
    applyTheme(GetThemeData(Name), Name)
end

return function(Name: string)
    table.clear(themeLineage)

    xpcall(applyThemeByName, function(err)
        ErrorMessage.Push(`Failed to apply theme: { err }`)
    end, Name)
end
