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

local function applyTheme(themeData: ThemeData, current: string)
    imgui.stylecolors(BaseImGuiStyles[themeData.ImGui.Base])

    for name, value in themeData.ImGui.Overrides do
        imgui.setstylecolor((Enum.ImGuiColor :: any)[name], value)
    end

    for key, value in themeData do
        if typeof(value) == "table" then
            Theme[key] = table.clone(value)
        else
            Theme[key] = value
        end
    end

    Theme.ImGui.Overrides = table.clone(Theme.ImGui.Overrides)
end

function applyThemeByName(Name: string)
    applyTheme(GetThemeData(Name), Name)
end

type Components = {
    Mesh: { Asset: string },
    Transform: { Position: vector },
}

type GameObject = {
    Name: string,
}

type function findComponentsFromUnionRecursive(un: type): { type }
    local results = {}

    for _, t in un:components() do
        if t:is("union") then
            for _, inner in findComponentsFromUnionRecursive(t) do
                table.insert(results, inner)
            end
        else
            local found = false

            -- Note: can't do direct index check because types won't be rawequal
            for i, c in Components:properties() do
                if i:value() == t:value() then
                    table.insert(results, c.read)
                    found = true
                    break
                end
            end

            if not found then
                --error(`invalid component '{t:value()}'`)
            end
        end
    end

    return results
end

return function(Name: string)
    xpcall(applyThemeByName, function(err)
        ErrorMessage.Push(`Failed to apply theme: { err }`)
    end, Name)
end
