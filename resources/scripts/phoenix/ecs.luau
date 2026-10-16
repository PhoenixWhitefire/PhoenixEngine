-- ecs.luau, 15/10/2026
--- An identifier for something in the ECS world.
export type Id<T = unknown> = {
    --- This is phantom, type system-only data, and does not exist at runtime.
    read _T: T
}
--- An entity identifier.
export type Entity = Id<nil>
--- A component identifier.
export type Component<T = unknown> = Id<T>
--- A pair identifier.
export type Pair = Id<nil>

--[[
type function quererTy(fnTy: type)
    assert(fnTy:is("function"))
    local head = assert(fnTy:parameters().head)
    local tys = {}
    tys[1] = head[1]

    local field = types.singleton("_T")

    for i = 2, #head do
        tys[i] = head[i]:readproperty(field)
    end

    return types.newfunction({ head = { head[1] } }, { head = tys })
end
]]--

--type QueryIteratorMeta<T...> = { read __iter: (QueryIterator<T...>) -> (quererTy<(Entity, T...) -> ()>, Entity) }
type QueryIteratorMeta<T...> = { read __iter: (QueryIterator<T...>) -> ((Entity) -> (Entity, T...), Entity) }
type QueryIterator<T...> = setmetatable<{}, QueryIteratorMeta<T...>>

--- The ECS world.
export type World = {
    --- Creates a new entity and returns its ID.
    read entity: (self: World) -> Entity,
    --- Creates a new component and returns its ID.
    read component: <T>(self: World) -> Component<T>,

    --- Adds a component or pair (relationship) to an entity.
    read add: (self: World, e: Entity, p: Component | Pair) -> (),
    --- Removes a component or pair (relationship) from an entity.
    read remove: (self: World, e: Entity, p: Component | Pair) -> (),

    --- Finds component data on an entity. Returns `nil` if the component is not present.
    --- Use `get` if you do not want this behavior.
    read find: <T>(self: World, e: Entity, c:Component<T>) -> T?,

    --- Gets component data on an entity. This will error if the component is not present.
    --- Use `find` if you do not want this behavior.
    read get: <T>(self: World, e: Entity, c: Component<T>) -> T,

    --- Sets component data on an entity. This will add the component if it is not present on `e`.
    read set: <T>(self: World, e: Entity, c: Component<T>, data: T) -> (),

    --- Queries the world for all entities matching the given predicates.
    --- Predicates are component or pair IDs, where pairs represent relationships.
    read query: <T...>(World, T...) -> QueryIterator<T...>
}

--- The ECS library.
local ecs = {}

--- Provides a pair identifier between two ECS identifiers.
function ecs.pair(predicate: Id, target: Id): Id
    error(`Unreachable.`)
end

return ecs
