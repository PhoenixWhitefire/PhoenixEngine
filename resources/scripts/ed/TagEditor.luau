-- TagEditor.luau, 01/02/2026
local DeveloperTools = game:GetService("DeveloperTools")
local Collections = game:GetService("Collections")

local Viewer = {}
Viewer.Open = false

function Viewer.Render()
    if not Viewer.Open then
        return
    end

    local shouldRender, open = imgui.begin("Collections", nil, true)
    if not shouldRender then
        imgui.endw()
        return
    end

    Viewer.Open = open

    local tags = Collections:GetCollections()
    table.sort(tags)

    for _, tag in tags do
        if imgui.treenode(tag, "ar | dlf") then
            if imgui.itemclicked() then
                DeveloperTools:SetExplorerSelections(Collections:GetTagged(tag))
            end

            for _, object in Collections:GetTagged(tag) do
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
