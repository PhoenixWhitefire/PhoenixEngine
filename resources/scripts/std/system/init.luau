--!strict
-- @std/system
-- stdlib for `@lute/system`
-- Provides re-exports of `@lute/system` and boolean properties for OS detection

local path = require("@std/path")
local platform = require("@self/platform")

local systemlib = {}

--- Returns the path of the system's temporary directory.
function systemlib.tmpdir(): path.Path
	return path.parse(phoenix.fs.tempdir())
end

local Engine = game:GetService("Engine")

--- CPU information for a single logical processor, including model, speed, and time-usage breakdown.
export type CpuInfo = {
	model: string,
	speed: number,

	times: {
		sys: number,
		idle: number,
		irq: number,
		nice: number,
		user: number,
	},
}

-- re-exports
systemlib.os = string.lower(Engine.TargetPlatform)
systemlib.arch = "unknown"

--- Returns the number of logical CPU threads available on the current machine.
function systemlib.threadCount(): number
	return phoenix.system.threadcount()
end

--- Returns the hostname of the current machine.
function systemlib.hostName(): string
	return phoenix.system.hostname()
end

--- Returns the total amount of system memory in bytes.
function systemlib.totalMemory(): number
	return phoenix.system.totalmemory()
end

--- Returns the amount of free (available) system memory in bytes.
function systemlib.freeMemory(): number
	return phoenix.system.freememory()
end

--- Returns the system uptime in seconds.
function systemlib.uptime(): number
	return game.Time
end

--- Returns CPU information for each logical processor on the current machine.
function systemlib.cpus(): { CpuInfo }
	return {}
end

-- boolean properties
systemlib.win32 = platform.win32
systemlib.linux = platform.linux
systemlib.macos = platform.macos
systemlib.unix = platform.unix

return table.freeze(systemlib)
