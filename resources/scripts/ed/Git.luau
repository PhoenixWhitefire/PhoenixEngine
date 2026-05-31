-- Git.luau, 30/05/2026
local Git = {
    Available = false,
    StatusResult = "",
    DiffResult = "",
}

function Git.Initialize()
    Git.Available = fs.execute("git --version"):find("git version") ~= nil
    print(if Git.Available then "Git is available" else "Git is not available")
end

return Git
