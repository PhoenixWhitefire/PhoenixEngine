local str = [[
]] -- copy enums from `glfw3.h` line 396 onwards. remove comments and whitespace newlines

local lastn = -1

local remap = {
    [0] = "Zero",
    [1] = "One",
    [2] = "Two",
    [3] = "Three",
    [4] = "Four",
    [5] = "Five",
    [6] = "Six",
    [7] = "Seven",
    [8] = "Eight",
    [9] = "Nine"
}

local print = appendlog or print

while #str > 1 do
    local knstart = #("#define GLFW_KEY_") + 1
    local keyname = str:sub(knstart, (assert((str:find(" ", knstart + 1)))) - 1)
    local numloc = assert((str:find(" %d+")))
    local num = str:sub(numloc, numloc+3)
    local numAsNumber = assert(tonumber(num))

    if lastn ~= (numAsNumber - 1) then
        for i = lastn + 1, numAsNumber - 1 do
            print(`    nullptr,`)
        end
    end

    lastn = numAsNumber

    if keyname:find("KP_") then
        keyname =  "NumPad_" .. keyname:sub(4, #keyname)
    end

    local undloc = keyname:find("_")
    keyname = keyname:lower()
    keyname = keyname:sub(1, 1):upper() .. keyname:sub(2, #keyname)
    while undloc do
        keyname = keyname:sub(1, undloc - 1) .. keyname:sub(undloc+1, undloc+1):upper() .. keyname:sub(undloc+2, #keyname)
        undloc = keyname:find("_")
    end

    local ktn = tonumber(keyname)
    if ktn and remap[ktn] then
        keyname = remap[ktn]
    end

    print(`    "{keyname}",`)

    str = str:sub(assert((str:find("\n"))) + 1, #str)
end

if game then
    game:Close()
end
