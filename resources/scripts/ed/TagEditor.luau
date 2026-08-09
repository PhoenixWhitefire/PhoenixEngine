-- TagEditor.luau, 01/02/2026
local DeveloperTools = game:GetService("DeveloperTools")
local Collections = game:GetService("Collections")

local State = require("@ed/State")

local Viewer = {}
Viewer.Open = false

function Viewer.Render()
    if not Viewer.Open then
        return
    end

    local shouldRender, open = imgui.begin("Collections", nil, true)
    Viewer.Open = open

    if not shouldRender or not open then
        imgui.endw()
        return
    end

    local tags = Collections:GetCollections()
    table.sort(tags)

    for _, tag in tags do
        if string.find(tag, "Phoenix_") ~= 0 and imgui.treenode(tag, "dlf | fw") then
            local tagged = Collections:GetTagged(tag)

            if imgui.itemclicked() then
                DeveloperTools:SetExplorerSelections(tagged)
            end

            for _, object in tagged do
                if imgui.textlink(object:GetFullName()) then
                    DeveloperTools:SetExplorerSelections({ object })
                end
            end

            imgui.treepop()
        end
    end

    imgui.endw()
end

return Viewer
