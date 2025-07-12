-- docs-ci.luau, 29/04/2025

print("Docs CI begin\n\n\n\n\n")

local apiDump = json_parse(file_read("./apidump.json"))
local docComments = json_parse(file_read("./doc-assets/ci/doc-comments.json"))

local globalsPrologue = file_read("./doc-assets/ci/globals-prologue.txt")
local globalsEpilogue = file_read("./doc-assets/ci/globals-epilogue.txt")
local componentsPrologue = file_read("./doc-assets/ci/components-prologue.txt")

local globals = globalsPrologue .. "\n\n"
local components = componentsPrologue .. "\n\n"

local function keysOf(t)
	local kt = {}
	for k, _ in t do
		table.insert(kt, k)
	end
	
	return kt
end

local orderedGlobals = keysOf(apiDump.ScriptGlobals)
table.sort(orderedGlobals)

for _, name in orderedGlobals do
	local contents = docComments.ScriptGlobals[name]
	
	if not contents then
		print(`Missing doc entry for global '{ name }'`)
		continue
	end
	
	if #(contents[1] or {}) < apiDump.ScriptGlobals[name] then
		print(`Expected at least { apiDump.ScriptGlobals[name] } args to be documented for global '{ name }', got { #contents[1] } instead`)
		continue
	end
	
	local args = ""
	for _, a in (contents[1] or {}) do
		args ..= `{ a[1] }: { a[2] }, `
	end
	args = args:sub(1, #args - 2)
	
	local returns = ""
	if #(contents[2] or {}) == 1 then
		returns =  " : " .. contents[2][1]
	elseif #(contents[2] or {}) > 1 then
		returns = " : ("
		
		for _, r in contents[2] do
			returns ..= r .. ", "
		end
		returns = returns:sub(1, #returns - 2)
		returns ..= ")"
	end
	
	globals ..= `#### \`{ name }({ args }){ returns }\`\n`
	
	for _, comment in (if typeof(contents[3]) == "table" then contents[3] else { contents[3] }) do
		if not comment then
			print(`Missing doc comment for global '{ name }'`)
		end
		
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

local function rtolsub(str)
	for p, s in rtol do
		str = string.gsub(str, p, s)
	end
	
	return str
end

local function count(t)
	local n = 0
	for _, _ in t do
		n += 1
	end
	return n
end

local function dumpReflectionApi(t, d, cn)
	local hasProps = t.Properties and (count(t.Properties) > 0)
	local hasFuncs = t.Functions and (count(t.Functions) > 0)
	local hasEvnts = t.Events and (count(t.Events) > 0)
	
	local s = ""
	
	if hasProps or hasFuncs or hasEvnts then
		if hasProps then
			s = "### Properties:\n"
			
			local orderedProps = keysOf(t.Properties)
			table.sort(orderedProps)
			
			for _, n in orderedProps do
				local v = t.Properties[n]
				
				if not d.Properties or not d.Properties[n] then
					print(`Missing doc comment for Property '{ cn }.{ n }'`)
					continue
				end
				
				s ..= `* \`{ n }: { rtol[v] or v }\`: { d.Properties[n] }\n`
			end
		end
		
		if hasFuncs then
			s ..= "\n### Methods:\n"
			
			local orderedFuncs = keysOf(t.Functions)
			table.sort(orderedFuncs)
			
			for _, n in orderedFuncs do
				local v = t.Functions[n]
				
				if not d.Functions or not d.Functions[n] then
					print(`Missing doc comment for Method '{ cn }:{ n }'`)
					continue
				end
				
				s ..= `* \`{ n }{ rtolsub(v) }\`: { d.Functions[n] }\n`
			end
		end
		
		if hasEvnts then
			s..= "\n### Events:\n"
			
			local orderedEvents = keysOf(t.Events)
			table.sort(orderedEvents)
			
			for _, n in orderedEvents do
				local v = t.Events[n]
				
				if not d.Events or not d.Events[n] then
					print(`Missing doc comment for Event '{ cn }.{ n }'`)
					continue
				end
				
				s ..= `* \`{ n }{ rtolsub(v) }\`: { d.Events[n] }\n`
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

local orderedComponents = keysOf(apiDump.GameObject.Components)
table.sort(orderedComponents)

for _, k in orderedComponents do
	local v = apiDump.GameObject.Components[k]
	
	components ..= `## \`{ k }\`\n\n`
	
	local vd = docComments.GameObject.Components[k]
	if not vd then
		print(`No docs for Component '{ k }'`)
		continue
	end
	
	if vd.Description then
		for _, d in (if typeof(vd.Description) == "table" then vd.Description else { vd.Description }) do
			components ..= `* { d }\n`
		end
		
		components ..= "\n"
	end
	
	components ..= dumpReflectionApi(v, docComments.GameObject.Components[k], k)
end

file_write("./doc-assets/ci/components.txt", components, false)

print("Docs CI finish\n\n\n\n\n")

-- close when we're done
game:Close()
