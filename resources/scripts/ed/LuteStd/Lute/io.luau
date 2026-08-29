local io = {}

--- Writes one or more strings to standard output without a trailing newline.
function io.write(...: string): ()
	for i = 1, select("#", ...) do
		appendlog(select(i, ...) .. "&&")
	end
end

--- Reads a line from standard input and returns it as a string.
function io.read(): string
	return ""
end

return io
