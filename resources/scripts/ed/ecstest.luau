-- ecstest.luau, 16/10/2026
local world = require("@phoenix/world")
local ecs = require("@phoenix/ecs")

local imgui = require("@phoenix/imgui")
local task = require("@std/task")

local Parent = world:component<<ecs.Id>>()
local Name = world:component<<string>>()

local root = world:entity()
local childA = world:entity()
local childB = world:entity()
local descendant = world:entity()

world:set(root, Name, "root")
world:set(childA, Name, "A")
world:set(childB, Name, "B")
world:set(descendant, Name, "descendant")

local childrenOfRoot = ecs.pair(Parent, root)
local childrenOfA = ecs.pair(Parent, childA)
world:add(childA, childrenOfRoot)
world:add(childB, childrenOfRoot)
world:add(descendant, childrenOfA)
world:set(childA, Parent, root)
world:set(childB, Parent, root)
world:set(descendant, Parent, childA)

print("Names:")

for e, name in world:query(Name) do
    print(`{e}: {name}`)
end

print("Children of root:")

for e, name, _ in world:query(Name, childrenOfRoot) do
    print(`{e}: {name}`)
end

local function renderEnt(e: ecs.Id)
    local name = world:find(e, Name)

    if name then
        return imgui.button(name)
    else
        return imgui.button(`<{ e }>`)
    end
end

local sel = nil

while true do
    imgui.begin("Entity report")

    for e in world:query() do
        if renderEnt(e) then
            sel = e
        end
    end

    if sel then
        imgui.pushid("dets")
        imgui.separatortext("Details")

        local name = world:find(sel, Name)
        if name then
            imgui.text("Name")
            imgui.sameline()

            local new, changed = imgui.inputstring("##Name", name)
            if changed then
                world:set(sel, Name, new)
            end
        end

        local parent = world:find(sel, Parent)
        if parent then
            imgui.text("Parent")
            imgui.sameline()

            if renderEnt(parent) then
                sel = parent
            end
        end

        imgui.popid()
    end

    imgui.endw()
    task.wait()
end
