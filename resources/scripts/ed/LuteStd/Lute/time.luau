-- lute-lint-global-ignore(unused_variable)
local time = {}

local duration_ops = {}
local instant = {}

local durationMeta: DurationMetaTy

local function durate(len: number): Duration
    return setmetatable({ length = len }, durationMeta)
end

durationMeta = {
    __metatable = "The metatable is locked!",
    __index = duration_ops,
    __add = function(a: Duration, b: Duration): Duration
        return durate(a.length + b.length)
    end,
    __sub = function(a: Duration, b: Duration): Duration
        return durate(a.length - b.length)
    end,
    __mul = function(a: Duration, b: number): Duration
        return durate(a.length * b)
    end,
    __div = function(a: Duration, b: number): Duration
        return durate(a.length / b)
    end,
    __eq = function(a: Duration, b: Duration): boolean
        return a.length == b.length
    end,
    __lt = function(a: Duration, b: Duration): boolean
        return a.length < b.length
    end,
    __le = function(a: Duration, b: Duration): boolean
        return a.length <= b.length
    end,
}

local instantMeta: InstantMetaTy = {
    __metatable = "The metatable is locked!",
    __index = instant,
    __sub = function(a: Instant, b: Instant): Duration
        return setmetatable({ length = a.point - b.point }, durationMeta)
    end,
    __eq = function(a: Instant, b: Instant): boolean
        return a.point == b.point
    end,
    __lt = function(a: Instant, b: Instant): boolean
        return a.point < b.point
    end,
    __le = function(a: Instant, b: Instant): boolean
        return a.point <= b.point
    end
}

--- Returns the current time as an `Instant`.
function time.now(): Instant
    return setmetatable({ point = os.time() }, instantMeta)
end

--- Returns the number of seconds elapsed since `instant`.
function time.since(instant: Instant): number
    return os.time() - instant.point
end

time.duration = {}

--- Creates a `Duration` from `seconds` and a fractional `subsecnanos` nanosecond component.
function time.duration.create(seconds: number, subsecnanos: number): Duration
    return durate(seconds + subsecnanos / 10^9)
end

--- Creates a `Duration` from a number of nanoseconds.
function time.duration.nanoseconds(nanoseconds: number): Duration
    return durate(nanoseconds / 10^9)
end

--- Creates a `Duration` from a number of microseconds.
function time.duration.microseconds(microseconds: number): Duration
    return durate(microseconds / 10^6)
end

--- Creates a `Duration` from a number of milliseconds.
function time.duration.milliseconds(milliseconds: number): Duration
    return durate(milliseconds / 10^3)
end

--- Creates a `Duration` from a number of seconds.
function time.duration.seconds(seconds: number): Duration
    return durate(seconds)
end

--- Creates a `Duration` from a number of minutes.
function time.duration.minutes(minutes: number): Duration
    return durate(minutes * 60)
end

--- Creates a `Duration` from a number of hours.
function time.duration.hours(hours: number): Duration
    return durate(hours * 3600)
end

--- Creates a `Duration` from a number of days.
function time.duration.days(days: number): Duration
    return durate(days * 86400)
end

--- Creates a `Duration` from a number of weeks.
function time.duration.weeks(weeks: number): Duration
    return durate(weeks * 86400 * 7)
end

function instant.elapsed(self: Instant): number
    return os.time() - self.point
end

function duration_ops.toNanoseconds(self: Duration): number
    return self.length * 10^9
end

function duration_ops.toMicroseconds(self: Duration): number
    return self.length * 10^6
end

function duration_ops.toMilliseconds(self: Duration): number
    return self.length * 10^3
end

function duration_ops.toSeconds(self: Duration): number
    return self.length
end

function duration_ops.toMinutes(self: Duration): number
    return self.length / 60
end

function duration_ops.toHours(self: Duration): number
    return self.length / 3600
end

function duration_ops.toDays(self: Duration): number
    return self.length / 86400
end

function duration_ops.toWeeks(self: Duration): number
    return self.length / 86400 / 7
end

function duration_ops.subsecnanos(self: Duration): number
    local nanos = self.length * 10^9
    return nanos - math.floor(self.length) * 10^9
end

function duration_ops.subsecmicros(self: Duration): number
    local micros = self.length * 10^6
    return micros - math.floor(self.length) * 10^6
end

function duration_ops.subsecmillis(self: Duration): number
    local millis = self.length * 10^3
    return millis - math.floor(self.length) * 10^3
end

type FrozenDuration = typeof(table.freeze({ length = 0 }))

type DurationMetaTy = {
    read __metatable: "The metatable is locked!",
    read __index: typeof(table.freeze(duration_ops)),
    read __add: (Duration, Duration) -> Duration,
    read __sub: (Duration, Duration) -> Duration,
    read __mul: (Duration, number) -> Duration,
    read __div: (Duration, number) -> Duration,
    read __eq: (Duration, Duration) -> boolean,
    read __lt: (Duration, Duration) -> boolean,
    read __le: (Duration, Duration) -> boolean,
}

--- A time duration with nanosecond precision. Supports arithmetic via `+`, `-`, `*`, `/`, and comparison operators.
export type Duration = setmetatable<FrozenDuration, DurationMetaTy>

type InstantMetaTy = {
    read __metatable: "The metatable is locked!",
    read __index: typeof(table.freeze(instant)),
    read __sub: (Instant, Instant) -> Duration,
    read __eq: (Instant, Instant) -> boolean,
    read __lt: (Instant, Instant) -> boolean,
    read __le: (Instant, Instant) -> boolean,
}

type FrozenInstant = typeof(table.freeze({ point = 0 }))

--- A point in time with nanosecond precision. Can be subtracted from another `Instant` to produce a `Duration`.
export type Instant = setmetatable<FrozenInstant, InstantMetaTy>

return time
