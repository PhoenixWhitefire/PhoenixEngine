type expectTable<T...> = {
    toBe: (expectTable<T...>, T...) -> (),
    toBeNil: (expectTable<T...>) -> (),
    toThrow: (expectTable<T...>, string?) -> (),
    toBeWithin: (expectTable<T...>, min: number, max: number) -> (),
}

export type Test = (string, () -> ()) -> ()
export type Expect = <T>(T) -> (expectTable<T> & { never: expectTable<T> })

return nil
