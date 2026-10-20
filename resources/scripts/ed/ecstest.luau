-- ecstest.luau, 16/10/2026
local world = require("@phoenix/world")
local ecs = require("@phoenix/ecs")

local imgui = require("@phoenix/imgui")
local task = require("@std/task")

local Health = world:component<<{ Current: number, read Max: number }>>()
local Strength = world:component<<number>>()

world:set(Health, ecs.Name, "Health")
world:set(Strength, ecs.Name, "Strength")

local root = world:entity()
local childA = world:entity()
local childB = world:entity()
local descendant = world:entity()

world:set(root, ecs.Name, "root")
world:set(childA, ecs.Name, "A")
world:set(childB, ecs.Name, "B")
world:set(descendant, ecs.Name, "descendant")

local childOfRoot = ecs.pair(ecs.ChildOf, root)
local childOfA = ecs.pair(ecs.ChildOf, childA)
world:add(childA, childOfRoot)
world:add(childB, childOfRoot)
world:add(descendant, childOfA)

world:set(childOfRoot, ecs.Name, "ChildOfRoot")

world:set(childA, ecs.Transform, { World = Matrix.translated(0, 1, 0), Local = Matrix.translated(1, 0, 0) })
world:set(childB, Strength, 50)
world:set(descendant, Health, { Current = 40, Max = 50 })

print("")
print("Names:")

for e, name in world:query(ecs.Name) do
    print(`* {e}: {name}`)
end

print("")

local function nameOf(e: ecs.ReadId)
    local name = world:find(e, ecs.Name)
    return name or `({ e })`
end

print("Children of root:")

for e, _ in world:query(ecs.pair(ecs.ChildOf, childA)) do
    print(`* {nameOf(e)}`)
end

print("")
print("Non-root:")

for e, _ in world:query(ecs.pair(ecs.ChildOf, ecs.Wildcard)) do
    print(`* {e} = {nameOf(e)}`)
end

print("")
print("All relationships targeting 'A':")

for e, _ in world:query(ecs.pair(ecs.Wildcard, childA)) do
    print(`* {nameOf(e)}`)
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

        local parent = world:find(sel, ecs.ChildOf)
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

            imgui.setnextitemwidth(50)
            local x, xChanged = imgui.inputnumber("##x", trans.World.Translation.x)
            imgui.sameline()
            imgui.setnextitemwidth(50)
            local y, yChanged = imgui.inputnumber("##y", trans.World.Translation.y)
            imgui.sameline()
            imgui.setnextitemwidth(50)
            local z, zChanged = imgui.inputnumber("##z", trans.World.Translation.z)

            if xChanged or yChanged or zChanged then
                trans.World = Matrix.translated(x, y, z)
            end

            imgui.setnextitemwidth(50)
            x, xChanged = imgui.inputnumber("##lx", trans.Local.Translation.x)
            imgui.sameline()
            imgui.setnextitemwidth(50)
            y, yChanged = imgui.inputnumber("##ly", trans.Local.Translation.y)
            imgui.sameline()
            imgui.setnextitemwidth(50)
            z, zChanged = imgui.inputnumber("##lz", trans.Local.Translation.z)

            if xChanged or yChanged or zChanged then
                trans.Local = Matrix.translated(x, y, z)
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
