-- lute-lint-global-ignore(unused_variable)
local time = require("./time")

local fs = {}

--- An open file handle returned by `fs.open`.
export type FileHandle = {
    fd: number,
    err: number,
}

--- The type of a filesystem entry:
--- - `"file"`: A regular file.
--- - `"dir"`: A directory.
--- - `"link"`: A symbolic link.
--- - `"fifo"`: A named pipe (FIFO).
--- - `"socket"`: A Unix domain socket.
--- - `"char"`: A character device.
--- - `"block"`: A block device.
--- - `"unknown"`: An entry whose type could not be determined.
export type FileType = "file" | "dir" | "link" | "fifo" | "socket" | "char" | "block" | "unknown"
--- How a file should be opened:
--- - `"r"`: Open for reading. Fails if the file does not exist.
--- - `"w"`: Open for writing; creates the file if absent, truncates it if present.
--- - `"x"`: Open for exclusive creation. Fails if the file already exists.
--- - `"a"`: Open for appending; creates the file if absent, writes go to the end.
--- - `"r+"`: Open for reading and writing. Fails if the file does not exist.
--- - `"w+"`: Open for reading and writing; creates the file if absent, truncates it if present.
--- - `"x+"`: Open for reading and exclusive creation. Fails if the file already exists.
--- - `"a+"`: Open for reading and appending; creates the file if absent, writes go to the end.
export type HandleMode = "r" | "w" | "x" | "a" | "r+" | "w+" | "x+" | "a+"

--- Metadata about a filesystem entry, such as size, type, and timestamps.
export type FileMetadata = {
    type: FileType,
    permissions: { readonly: boolean },
    size: number,
    created: time.Duration,
    accessed: time.Duration,
    modified: time.Duration,
}

--- A single entry returned by `fs.listdir`, containing the entry's name and type.
export type DirectoryEntry = {
    name: string,
    type: FileType,
}

--- A handle to an active filesystem watcher, returned by `fs.watch`.
export type WatchHandle = {
    close: (self: WatchHandle) -> (),
}

--- A filesystem change event passed to the `fs.watch` callback.
export type WatchEvent = {
    change: boolean,
    rename: boolean,
}

local handles: { string } = {}

--- Opens the file at `path` in the given `mode` (defaults to `"r"`). Returns a file handle.
function fs.open(path: string, mode: HandleMode?): FileHandle
    table.insert(handles, path)

    return {
        fd = #handles,
        err = 0
    }
end

--- Reads the full contents of `handle` and returns them as a string.
function fs.read(handle: FileHandle): string
    return phoenix.fs.read(handles[handle.fd]) or ""
end

--- Writes `contents` to `handle`.
function fs.write(handle: FileHandle, contents: string): ()
    phoenix.fs.write(handles[handle.fd], contents)
end

--- Closes `handle`, flushing any pending writes.
function fs.close(handle: FileHandle): ()
    handles[handle.fd] = nil
end

--- Reads up to `numBytes` bytes from `handle` at `fileOffset` into `buf` starting at `bufferOffset`.
--- Returns the number of bytes actually read (may be less than `count` if EOF is reached).
--- - `bufferOffset` defaults to 0.
function fs.readIntoBuffer(
    handle: FileHandle,
    buf: buffer,
    fileOffset: number,
    numBytes: number,
    bufferOffset: number?
): number
    local contents, err = phoenix.fs.read(handles[handle.fd])
    assert(contents, err)

    local toCopyStr = string.sub(contents, fileOffset, numBytes)
    local toCopy =  buffer.fromstring(toCopyStr)

    buffer.copy(buf, bufferOffset or 0, toCopy, fileOffset, #toCopyStr)
    return math.min(numBytes, #toCopyStr)
end

--- Removes the file at `path`.
function fs.remove(path: string): ()
    phoenix.fs.remove(path)
end

--- Returns metadata for the file or directory at `path`.
function fs.stat(path: string): FileMetadata
    local contents, err = phoenix.fs.read(path)
    assert(contents, err)

    local isWriteable = pcall(phoenix.fs.write, path, contents, nil)
    local modified = phoenix.fs.lastwritten(path)

    return {
        type = fs.type(path),
        permissions = { readonly = not isWriteable },
        size = #contents,
        created = time.duration.seconds(modified),
        accessed = time.duration.seconds(modified),
        modified = time.duration.seconds(modified),
    }
end

--- Returns the `FileType` of the entry at `path`.
function fs.type(path: string): FileType
    return if phoenix.fs.isfile(path) then "file"
        else if phoenix.fs.isdirectory(path) then "dir"
        else if phoenix.fs.issymlink(path) then "link"
        else "unknown"
end

--- Creates a directory at the specified path.
--- To set the permissions mode for a directory (Unix only), see @std/process for `run` or `system` to shell out to `chmod` or the equivalent.
function fs.mkdir(path: string): ()
    phoenix.fs.mkdir(path)
end

--- Creates a hard link at `dest` pointing to `src`.
function fs.link(src: string, dest: string): ()
    phoenix.fs.symlink(dest, src)
end

--- Creates a symbolic link at `dest` pointing to `src`.
function fs.symlink(src: string, dest: string): ()
    phoenix.fs.symlink(dest, src)
end

--- Watches `path` for filesystem changes, calling `callback` with the filename and event on each change.
function fs.watch(path: string, callback: (filename: string, event: WatchEvent) -> ()): WatchHandle
    local disconnect = phoenix.fs.watch(path, function(Path, Event)
        callback(path, {
            change = Event == Enum.FileSystemEvent.Modified,
            rename = Path ~= path
        })
    end)

    return {
        close = function()
            disconnect()
        end,
    }
end

--- Returns `true` if a file or directory exists at `path`.
function fs.exists(path: string): boolean
    return fs.type(path) ~= "unknown"
end

--- Copies the file at `src` to `dest`.
function fs.copy(src: string, dest: string): ()
    phoenix.fs.copy(src, dest)
end

--- Moves the file or directory at `src` to `dest`.
--- Falls back to a copy-then-remove if `src` and `dest` are on different filesystems.
function fs.move(src: string, dest: string): ()
    phoenix.fs.move(src, dest)
end

--- Returns an array of `DirectoryEntry` values for the immediate children of the directory at `path`.
function fs.listdir(path: string): { DirectoryEntry }
    local dirEnts: { DirectoryEntry } = {}

    for p, t in phoenix.fs.listdir(path) do
        table.insert(dirEnts, {
            name = p,
            -- TODO luau cast
            type = (if t == "d" then "dir" else "file") :: FileType
        })
    end

    return dirEnts
end

--- Removes the directory at `path`.
function fs.rmdir(path: string): ()
    if not phoenix.fs.isdirectory(path) then
        error(`{ path } does not refer to a directory`)
    end

    phoenix.fs.remove(path)
end

return fs
