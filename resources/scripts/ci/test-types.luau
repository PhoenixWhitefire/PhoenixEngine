type expectTable<T...> = {
    toBe: (expectTable<T...>, T...) -> (),
    toBeNil: (expectTable<T...>) -> (),
    toThrow: (expectTable<T...>, string?) -> ()
}

export type Test = (string, () -> ()) -> ()
export type Expect = <T>(T) -> (expectTable<T> & { never: expectTable<T> })

return nil
