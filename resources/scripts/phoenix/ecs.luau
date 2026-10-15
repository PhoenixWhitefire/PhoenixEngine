-- ecs.luau, 15/10/2026
export type Id<T = unknown> = { read _T: unknown }

type QueryIteratorMeta<T...> = { read __iter: (QueryIterator<T...>) -> ((nil, Id) -> (Id, T...), nil, Id) }
type QueryIterator<T...> = setmetatable<{}, QueryIteratorMeta<T...>>

local p = pairs

export type World = {
    read entity: (World) -> Id,
    read component: (World) -> Id,
    read query: <T...>(World, T...) -> QueryIterator<T...>
}

local ecs = {}

function ecs.pair(predicate: Id, target: Id): Id
    error(`Unreachable.`)
end

return ecs
