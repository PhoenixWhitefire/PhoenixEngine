-- docs-ci.luau, 29/04/2025

local apiDump = json_parse(file_read("./apidump.json"))
local docComments = json_parse(file_read("./doc-assets/ci/doc-comments.json"))

local globalsPrologue = file_read("./doc-assets/ci/globals-prologue.txt")
local globalsEpilogue = file_read("./doc-assets/ci/globals-epilogue.txt")
local componentsPrologue = file_read("./doc-assets/ci/components-prologue.txt")

local globals = globalsPrologue .. "\n\n"
local components = componentsPrologue .. "\n\n"

for _, def in docComments.ScriptGlobals do
	local name, contents = def[1], def[2]
	
	assert(#contents == 3, name .. " " .. #contents .. " " .. json_dump(contents))
	
	local args = ""
	for _, a in contents[1] do
		args ..= `{ a[1] }: { a[2] }, `
	end
	args = args:sub(1, #args - 2)
	
	local returns = ""
	if #contents[2] == 1 then
		returns =  " : " .. contents[2][1]
	elseif #contents[2] > 1 then
		returns = " : ("
		
		for _, r in contents[2] do
			returns ..= r .. ", "
		end
		returns = returns:sub(1, #returns - 2)
		returns ..= ")"
	end
	
	globals ..= `#### \`{ name }({ args }){ returns }\`\n`
	
	for _, comment in (if typeof(contents[3]) == "table" then contents[3] else { contents[3] }) do
		globals ..= `* { comment }\n`
	end
	
	globals ..= "\n"
end

globals ..= globalsEpilogue

local rtol = {
	Bool = "boolean",
	Integer = "number",
	Double = "number",
	String = "string"
}

local function count(t)
	local n = 0
	for _, _ in t do
		n += 1
	end
	return n
end

local function dumpReflectionApi(t, d)
	local hasProps = t.Properties and (count(t.Properties) > 0)
	local hasFuncs = t.Functions and (count(t.Functions) > 0)
	
	local s = ""
	
	if hasProps or hasFuncs then
		if hasProps then
			s = "### Properties:\n"
			
			for n, v in t.Properties do
				s ..= `* \`{ n }: { rtol[v] or v }\`: { d.Properties[n] }\n`
			end
		end
		
		if hasFuncs then
			s ..= "\n### Functions:\n"
			
			for n, v in t.Functions do
				s ..= `* \`{ n }{ v }\`: { d.Functions[n] }\n`
			end
		end
	else
		s = "No members defined\n"
	end
	
	s ..= "\n"
	
	return s
end

globals ..= dumpReflectionApi(apiDump.GameObject.Base, docComments.GameObject.Base)

file_write("./doc-assets/ci/globals.txt", globals, false)

for k, v in apiDump.GameObject.Components do
	components ..= `## \`{ k }\`\n\n`
	
	local vd = docComments.GameObject.Components[k]
	
	if vd.Description then
		for _, d in (if typeof(vd.Description) == "table" then vd.Description else { vd.Description }) do
			components ..= `* { d }\n`
		end
		
		components ..= "\n"
	end
	
	components ..= dumpReflectionApi(v, docComments.GameObject.Components[k])
end

file_write("./doc-assets/ci/components.txt", components, false)

-- close when we're done
game:Close()
