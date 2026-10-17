-- ecs.luau, 15/10/2026
--- An identifier for something in the ECS world.
export type Id<T = unknown> = {
    --- This is phantom, type system-only data, and does not exist at runtime.
    _T: T
}
--- An entity identifier.
export type Entity = Id<nil>

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
    read component: <T>(self: World) -> Id<T>,

    --- Adds a component or pair (relationship) to an entity.
    read add: <T>(self: World, e: Entity, p: T & Id<nil>) -> (),
    --- Removes a component or pair (relationship) from an entity.
    read remove: (self: World, e: Entity, p: Id) -> (),

    --- Finds component data on an entity. Returns `nil` if the component is not present.
    --- Use `get` if you do not want this behavior.
    read find: <T>(self: World, e: Entity, c: Id<T>) -> T?,

    --- Gets component data on an entity. This will error if the component is not present.
    --- Use `find` if you do not want this behavior.
    read get: <T>(self: World, e: Entity, c: Id<T>) -> T,

    --- Sets component data on an entity. This will add the component if it is not present on `e`.
    read set: <T>(self: World, e: Entity, c: Id<T>, data: T) -> (),

    -- TODO: Replace this with the type function once the bug is fixed.
    --- Queries the world for all entities matching the given predicates.
    --- Predicates are component or pair IDs, where pairs represent relationships.
    read query:
        & ((World) -> QueryIterator<>)
        & (<A>(World, A) -> QueryIterator<index<A, "_T">>)
        & (<A, B>(World, A, B) -> QueryIterator<index<A, "_T">, index<B, "_T">>)
        & (<A, B, C>(World, A, B, C) -> QueryIterator<index<A, "_T">, index<B, "_T">, index<C, "_T">>)
        & (<A, B, C, D>(World, A, B, C, D) -> QueryIterator<index<A, "_T">, index<B, "_T">, index<C, "_T">, index<D, "_T">>)
        & (<A, B, C, D, E>(World, A, B, C, D, E) -> QueryIterator<index<A, "_T">, index<B, "_T">, index<C, "_T">, index<D, "_T">, index<E, "_T">>)
        & (<A, B, C, D, E, F>(World, A, B, C, D, E, F) -> QueryIterator<index<A, "_T">, index<B, "_T">, index<C, "_T">, index<D, "_T">, index<E, "_T">, index<F, "_T">>)
        & (<A, B, C, D, E, F, G>(World, A, B, C, D, E, F, G) -> QueryIterator<index<A, "_T">, index<B, "_T">, index<C, "_T">, index<D, "_T">, index<E, "_T">, index<F, "_T">, index<G, "_T">>)
}

--- The ECS library.
local ecs = {}

--- Provides a pair identifier between two ECS identifiers.
function ecs.pair<T, U>(predicate: Id<T>, target: Id<U>): Id<nil>
    error(`Unreachable.`)
end

return ecs
