--!optimize 0
task.wait(0.5)
debug.breakpoint(5)

local var1 = 1
var1 = 2
var1 = 3
local tab = { "Hello", "This is an array-style table!" }
tab[1] = var1
local dict = { ThisIsADictionary = true }
dict.Table = tab

local function foo()
	dict.Table[2] = "Hello, World!"
end

foo()
print("Done")
