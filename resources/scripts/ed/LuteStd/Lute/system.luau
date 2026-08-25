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
	return 8
end

--- Returns the hostname of the current machine.
function system.hostName(): string
	return phoenix.fs.home()
end

--- Returns the path to the system's temporary directory.
function system.tmpdir(): string
	return if system.os == "windows" then "%temp%"
		else "/tmp"
end

--- Returns the total amount of system memory in bytes.
function system.totalMemory(): number
	return 8 * 10^9
end

--- Returns the amount of free (available) system memory in bytes.
function system.freeMemory(): number
	return system.totalMemory() / 2
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
