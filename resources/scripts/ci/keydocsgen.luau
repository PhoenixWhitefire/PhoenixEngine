local BASE_PATH = "./wikigen"

local docComments = json.parse(assert(fs.read(BASE_PATH .. "/doc-comments.json"))) :: { [string]: any }
local keys = docComments.ScriptEnv.Libraries.Enum.Members.Key
keys.Items = {}

local function splitUppercase(str)
    local results = {}
    for word in str:gmatch("%u%U*") do
        table.insert(results, word)
    end
    return results
end

for k, _ in Enum.Key :: { [string]: unknown } do
    keys.Items[k] = `The {table.concat(splitUppercase(k), " ")} key`
end

fs.write(BASE_PATH .. "/doc-comments.json", json.encode(docComments))

game:GetService("Engine"):Exit(0)
