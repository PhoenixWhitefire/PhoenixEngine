-- ecstest.luau, 16/10/2026
local world = require("@phoenix/world")
local ecs = require("@phoenix/ecs")

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

print("Names:")

for e, name in world:query(Name) do
    print(`{e}: {name}`)
end

print("Children of root:")

for e, name, _ in world:query(Name, childrenOfRoot) do
    print(`{e}: {name}`)
end
