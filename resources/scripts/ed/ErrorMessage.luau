-- ErrorMessage.luau, 30/05/2026
local ErrorMessage = {
    Queue = {},
}

function ErrorMessage.Push(Message: string)
    table.insert(ErrorMessage.Queue, { Message = Message, TimeRemaining = 2 / math.max(#ErrorMessage.Queue, 0) })
    warn(Message)
end

return ErrorMessage
