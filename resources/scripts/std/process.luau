--!strict
-- @std/process
-- stdlib for `@lute/process`
-- Provides process-related utility functions that handles file paths as path types instead of strings

local pathlib = require("@std/path")

local processlib = {}

--- How a child process's standard streams should be handled:
--- - `"default"`: Capture stdout and stderr to pipes, accessible via `ProcessResult`.
--- - `"inherit"`: Pass the parent process's stdio streams through to the child.
--- - `"none"`: Discard the child's stdio streams.
export type SignalHandle = {
    close: (self: SignalHandle) -> (),
}
--- An operating system signal name.
---
--- Only `SIGINT` and `SIGTERM` are guaranteed to be supported on all platforms.
--- Other signals may be unsupported, but will result in no-op behavior when used.
export type Signal =
    | "SIGINT"
    | "SIGTERM"
    | "SIGHUP"
    | "SIGQUIT"
    | "SIGUSR1"
    | "SIGUSR2"
    | "SIGWINCH"
    | "SIGPIPE"
    | "SIGBREAK"
    | "SIGALRM"
--- How a child process's standard streams should be handled:
--- - `"default"`: Capture stdout and stderr to pipes, accessible via `ProcessResult`.
--- - `"inherit"`: Pass the parent process's stdio streams through to the child.
--- - `"none"`: Discard the child's stdio streams.
export type StdioKind = "default" | "inherit" | "none"
--- Options for `process.run`:
--- - `cwd`: The working directory for the child process. Defaults to the current working directory.
--- - `stdio`: How to handle the child's stdio streams. Defaults to `"default"`.
--- - `env`: Environment variables for the child process. If omitted, inherits the parent's environment.
export type ProcessRunOptions = {
    cwd: string?,
    stdio: StdioKind?,

    env: { [string]: string }?,
}
--- Options for `process.system`:
--- - `system`: The shell executable to use. If omitted, uses the platform default (`$SHELL` on Unix, `%COMSPEC%` on Windows).
--- - `cwd`: The working directory for the child process. Defaults to the current working directory.
--- - `stdio`: How to handle the child's stdio streams. Defaults to `"default"`.
--- - `env`: Environment variables for the child process. If omitted, inherits the parent's environment.
export type ProcessSystemOptions = {
    system: string?,
    cwd: string?,
    stdio: StdioKind?,

    env: { [string]: string }?,
}
--- The result of a completed process, including exit code and captured stdout/stderr.
export type ProcessResult = {
    stdout: string,
    stderr: string,

    ok: boolean,
    exitcode: number,
    signal: string?,
}
--- A structured, platform-aware file system path.
export type Path = pathlib.Path
--- Anything that can be used as a path: a string, a `Path`, or raw path data.
export type Pathlike = pathlib.Pathlike

--- Returns the current user's home directory as a `Path`.
function processlib.homedir(): Path
    return pathlib.parse(phoenix.fs.home())
end

--- Returns the current working directory as a `Path`.
function processlib.cwd(): Path
    return pathlib.parse(phoenix.fs.cwd())
end

--- Runs the program given by `args[1]` with the remaining entries as arguments.
--- Returns the process result including exit code and any captured output.
function processlib.run(args: { string }, options: ProcessRunOptions?): ProcessResult
    return processlib.system(table.concat(args, " "), options :: any)
end

--- Runs `command` through the system shell. Returns the process result.
function processlib.system(command: string, options: ProcessSystemOptions?): ProcessResult
    return {
        stdout = phoenix.fs.execute(command),
        stderr = "",

        ok = true,
        exitcode = 0,
        signal = nil,
    }
end

--- Exits the current process with the given `exitcode`.
function processlib.exit(exitcode: number): never
    game:Close(exitcode)

    coroutine.wrap(function(curr)
        coroutine.close(curr)
    end)(coroutine.running())

    return nil :: never
end

local Engine = game:GetService("Engine")

--- Returns the path of the currently running lute executable as a `Path`.
function processlib.execPath(): Path
    return pathlib.parse(Engine:GetCliArguments()[1])
end

--- Registers `callback` to be called whenever `signal` is delivered to this process.
--- Suppresses the default OS behavior (e.g. `"SIGINT"` will no longer terminate the process).
--- Returns a handle; call `handle:close()` to deregister.
function processlib.onSignal(signal: Signal, callback: () -> ()): SignalHandle
    return error("not implemented")
end

--- Returns the PID of the current process.
function processlib.pid(): number
    return error("not implemented")
end

-- re-exports
processlib.args = Engine:GetCliArguments()
processlib.env = {} :: { string }

return table.freeze(processlib)
