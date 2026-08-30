local Engine = game:GetService("Engine")

local osName = string.lower(Engine.TargetPlatform)

local win32 = osName == "windows_nt" or osName == "windows"
local linux = osName == "linux"
local macos = osName == "darwin"
local unix = macos or linux or osName == "unix" or osName == "posix"

return table.freeze({ win32 = win32, linux = linux, macos = macos, unix = unix })
