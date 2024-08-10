-- devmap.luau, 10/07/2024

local function printTable(tbl, depth)
    depth = depth or 0
    print(string.rep("    ", depth), "{")
    for k, v in tbl do
        if typeof(v) == "table" then
            print(k, "=")
            printTable(v, depth + 1)
            continue
        end
        print(string.rep("    ", depth + 1), k, "=", v)
    end
    print(string.rep("    ", depth), "}")
end

printTable(game)