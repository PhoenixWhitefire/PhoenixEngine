-- parallel.luau, 28/05/2026
local Types = require("@ci/test-types")

local SCRIPT = "scripts/ci/tests/libraries/parallel.luau"

local NWORKERS = 8
local workerId = (...)

if typeof(workerId) == "number" then
    local sharedBuffer = parallel.sharedbuffer("BufferBar", NWORKERS + 4)
    local int = parallel.atomicinteger(0)

    local mutex = parallel.mutex("MutexFoo")

    local curSlice = sharedBuffer:Pull(workerId - 1, 1)
    buffer.writeu8(curSlice, 0, workerId)
    sharedBuffer:Push(curSlice, workerId - 1)

    mutex:Lock()

    local criticalFooter = sharedBuffer:Pull(NWORKERS, 4)

    buffer.writeu32(criticalFooter, 0, workerId)
    sharedBuffer:Push(criticalFooter, NWORKERS)

    mutex:Unlock()

    int:Set(int:Get() - 1)
end

local function parallelTests(test: Types.Test, expect: Types.Expect)
    assert(workerId == nil)

    test("parallel.atomicinteger, .sharedbuffer, .mutex", function()
        for i = 1, NWORKERS do
            parallel.spawn(SCRIPT, i)
        end

        local sharedBuffer = parallel.sharedbuffer("BufferBar", NWORKERS + 4)
        local int = parallel.atomicinteger(0)

        int:Set(NWORKERS)

        repeat
            task.wait(0.1)
        until int:Get() == 0

        local buf = sharedBuffer:Pull()

        for i = 1, NWORKERS do
            expect(buffer.readu8(buf, i - 1)):toBe(i)
        end

        expect(buffer.readu32(buf, NWORKERS)):toBeWithin(1, NWORKERS)
    end)
end

return parallelTests
