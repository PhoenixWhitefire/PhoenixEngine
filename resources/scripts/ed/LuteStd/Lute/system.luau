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

local system = {}

--- Returns the number of logical CPU threads available on the current machine.
function system.threadCount(): number
	return phoenix.system.threadcount()
end

--- Returns the hostname of the current machine.
function system.hostName(): string
	return phoenix.system.hostname()
end

--- Returns the path to the system's temporary directory.
function system.tmpdir(): string
	return phoenix.fs.tempdir()
end

--- Returns the total amount of system memory in bytes.
function system.totalMemory(): number
	return phoenix.system.totalmemory()
end

--- Returns the amount of free (available) system memory in bytes.
function system.freeMemory(): number
	return phoenix.system.freememory()
end

--- Returns the system uptime in seconds.
function system.uptime(): number
	return game.Time
end

--- Returns CPU information for each logical processor on the current machine.
function system.cpus(): { CpuInfo }
	return {}
end

local Engine = game:GetService("Engine")

--- The current operating system, e.g. `"linux"`, `"macos"`, `"windows"`.
system.os = string.lower(Engine.TargetPlatform)
--- The CPU architecture of the current machine, e.g. `"x64"`, `"arm64"`.
system.arch = "unknown"

return system
