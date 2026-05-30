-- ErrorMessage.luau, 30/05/2026
local ErrorMessage = {
    Queue = {},
}

function ErrorMessage.Push(Message: string)
    table.insert(ErrorMessage.Queue, { Message = Message, TimeRemaining = 5 })
end

return ErrorMessage
