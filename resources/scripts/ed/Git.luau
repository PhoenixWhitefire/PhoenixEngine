-- Git.luau, 30/05/2026
local Git = {
    Available = fs.execute("git --version"):find("git version") ~= nil,
    StatusResult = "",
    DiffResult = "",
}

print(if Git.Available then "Git is available" else "Git is not available")
return Git
