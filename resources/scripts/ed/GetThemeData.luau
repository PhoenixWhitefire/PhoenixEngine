-- GetThemeData.luau, 06/06/2026
local DefaultThemes = require("@ed/DefaultThemes")
local ErrorMessage = require("@ed/ErrorMessage")

type ThemeData = index<typeof(DefaultThemes), string>

local themeLineage = {}

local function getThemeData(Name: string): ThemeData
    local data: ThemeData?

    if DefaultThemes[Name] then
        data = table.clone(DefaultThemes[Name])
        data.ImGui = {
            Base = DefaultThemes[Name].ImGui.Base,
            Overrides = table.clone(DefaultThemes[Name].ImGui.Overrides)
        }

        for k, v in data do
            if k ~= "ImGui" and typeof(v) == "table" then
                data[k] = table.clone(v)
            end
        end
    else
        local themeContents, err = fs.read(`./editor/{ Name }`)
        if not themeContents then
            ErrorMessage.Push(`Failed to load theme file: { err }`)
            return DefaultThemes.Dark
        end

        data = json.parse(themeContents) :: ThemeData
    end

    if data.Inherit then
        if table.find(themeLineage, Name) then
            local lineage = ""

            for _, theme in themeLineage do
                lineage ..= `{ theme } -> `
            end
            lineage ..= Name

            ErrorMessage.Push(`{ Name } relies on itself! { lineage }`)
            return data
        end

        table.insert(themeLineage, 1, Name)
        local inherited = getThemeData(data.Inherit)

        for k, v in inherited do
            if k == "ImGui" or typeof(v) ~= "table" then
                continue
            end

            if not data[k] then
                data[k] = v
            end
        end

        for k, v in inherited.ImGui.Overrides do
            if not data.ImGui.Overrides[k] then
                data.ImGui.Overrides[k] = v
            end
        end
    end

    return data
end

return function(Name: string): ThemeData
    table.clear(themeLineage)
    return getThemeData(Name)
end
