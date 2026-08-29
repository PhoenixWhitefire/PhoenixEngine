-- Git.luau, 30/05/2026
local Git = {
    Available = false,
    StatusResult = "",
    DiffResult = "",
}

local process = require("@std/process")

function Git.Initialize()
    Git.Available = process.system("git --version").stdout:find("git version") ~= nil
    print(if Git.Available then "Git is available" else "Git is not available")
end

function Git.Command(Command)
    return process.system(`git -C "{ phoenix.fs.resolvepathabsolute("@project") }" { Command }`).stdout
end

return Git
