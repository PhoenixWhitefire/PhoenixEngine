-- ecstest.luau, 16/10/2026
local world = require("@phoenix/world")
local ecs = require("@phoenix/ecs")

local imgui = require("@phoenix/imgui")
local task = require("@std/task")

local Parent = world:component<<ecs.Id>>("Parent")
local Health = world:component<<{ Current: number, read Max: number }>>("Health")
local Strength = world:component<<number>>("Strength")

local root = world:entity()
local childA = world:entity()
local childB = world:entity()
local descendant = world:entity()

world:set(root, ecs.Name, "root")
world:set(childA, ecs.Name, "A")
world:set(childB, ecs.Name, "B")
world:set(descendant, ecs.Name, "descendant")

local childrenOfRoot = ecs.pair(Parent, root)
local childrenOfA = ecs.pair(Parent, childA)
world:add(childA, childrenOfRoot)
world:add(childB, childrenOfRoot)
world:add(descendant, childrenOfA)
world:set(childA, Parent, root)
world:set(childB, Parent, root)
world:set(descendant, Parent, childA)

world:set(childA, ecs.Transform, Matrix.translated(0, 1, 0))
world:set(childB, Strength, 50)
world:set(descendant, Health, { Current = 40, Max = 50 })

print("Names:")

for e, name in world:query(ecs.Name) do
    print(`{e}: {name}`)
end

print("Children of root:")

for e, name, _ in world:query(ecs.Name, childrenOfRoot) do
    print(`{e}: {name}`)
end

local function renderEnt(e: ecs.Id)
    local name = world:find(e, ecs.Name)

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

        local name = world:find(sel, ecs.Name)
        if name then
            imgui.text("Name")
            imgui.sameline()

            local new, changed = imgui.inputstring("##Name", name)
            if changed then
                world:set(sel, ecs.Name, new)
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

        local trans = world:find(sel, ecs.Transform)
        if trans then
            imgui.text("Transform")
            imgui.sameline()

            local x, xChanged = imgui.inputnumber("##x", trans.Translation.x)
            local y, yChanged = imgui.inputnumber("##y", trans.Translation.y)
            local z, zChanged = imgui.inputnumber("##z", trans.Translation.z)

            if xChanged or yChanged or zChanged then
                world:set(sel, ecs.Transform, Matrix.translated(x, y, z))
            end
        end

        local health = world:find(sel, Health)
        if health then
            imgui.text("Health")
            imgui.indent(20)

            imgui.text("Current:")
            imgui.sameline()
            health.Current = imgui.inputnumber("##current", health.Current)

            imgui.text(`Max: { health.Max }`)

            imgui.indent(-20)
        end

        local strength = world:find(sel, Strength)
        if strength then
            imgui.text("Strength:")
            imgui.sameline()

            local new, changed = imgui.inputnumber("##strength", strength)
            if changed then
                world:set(sel, Strength, new)
            end
        end

        imgui.popid()
    end

    imgui.endw()
    task.wait()
end
