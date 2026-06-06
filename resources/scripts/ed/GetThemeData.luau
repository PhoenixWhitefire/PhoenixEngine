-- GetThemeData.luau, 06/06/2026
local DefaultThemes = require("@ed/DefaultThemes")
local ErrorMessage = require("@ed/ErrorMessage")

local function getThemeData(Name: string): index<typeof(DefaultThemes), string>
    if DefaultThemes[Name] then
        return DefaultThemes[Name]
    else
        local themeContents, err = fs.read(`./editor/{ Name }`)
        if not themeContents then
            ErrorMessage.Push(`Failed to load theme file: { err }`)
            return DefaultThemes.Dark
        end

        local themeData = json.parse(themeContents)
        return themeData :: any
    end
end

return getThemeData
