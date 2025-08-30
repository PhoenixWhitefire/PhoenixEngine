-- docs-ci.luau, 29/04/2025

print("Docs CI begin\n\n\n\n\n")

local BASE_PATH = "./gh-assets/wiki"

local apiDump = json.parse(fs.read("./apidump.json"))
local docComments = json.parse(fs.read(BASE_PATH .. "/doc-comments.json"))

local datatypesPrologue = fs.read(BASE_PATH .. "/datatypes-prologue.txt")
local librariesPrologue = fs.read(BASE_PATH .. "/libraries-prologue.txt")
local globalsPrologue = fs.read(BASE_PATH .. "/globals-prologue.txt")
local componentsPrologue = fs.read(BASE_PATH .. "/components-prologue.txt")

local datatypes = ""
local libraries = ""
local globals = ""
local components = componentsPrologue .. "\n\n"

local function keysOf(t)
	local kt = {}
	for k, _ in t do
		table.insert(kt, k)
	end
	
	return kt
end

local orderedDatatypes = keysOf(apiDump.ScriptEnv.Datatypes)
local orderedLibraries = keysOf(apiDump.ScriptEnv.Libraries)
local orderedGlobals = keysOf(apiDump.ScriptEnv.Globals)
table.sort(orderedDatatypes)
table.sort(orderedLibraries)
table.sort(orderedGlobals)

local function dumpLibMembers(libm, t, libname)
	local str = ""
	
	for k, v in libm do
		if t[k] == "function" then
			local args = v.In or ""
			local returns = v.Out or ""
			
			if typeof(v) == "string" then
				args = "..."
				returns = "..."
			end
			
			if k == "new" and not v.Out then
				returns = libname
			end
			
			str ..= `#### \`{libname}.{ k }({ args }): { returns }\`\n`
		else
			str ..= `#### \`{libname}.{ k }: { v.Type }\`\n`
		end
		
		local desc = v.Description
		if not desc and typeof(v) == "string" then
			desc = v
		end
		
		if not desc then
			print(`No doc for {libname}.{k}`)
		end
		
		for _, comment in (if typeof(desc) == "table" then desc else { desc }) do
			if not comment then
				print(`Missing doc comment for '{ k }'`)
			end
		
			str ..= `* { comment }\n`
		end
		
		str ..= "\n"
	end
	
	return str
end

for _, name in orderedDatatypes do
	local dt = docComments.ScriptEnv.Datatypes[name]
	datatypes ..= `### \`{ name }\`\n\n`
	
	for _, d in (if typeof(dt.Description) == "table" then dt.Description else { dt.Description }) do
		datatypes ..= `* { d }\n`
	end
	
	datatypes ..= "\n" .. dumpLibMembers(dt.Library, apiDump.ScriptEnv.Datatypes[name].Library, name)
	
	for k, v in (dt.Members or {}) do
		if string.sub(k, 1) == ":" then
			local args = v.In or "..."
			local returns = v.Out or "..."
			
			datatypes ..= `#### \`{ k }({ args }): { returns }\`\n`
		else
			datatypes ..= `#### \`.{ k }: { v.Type }\`\n`
		end
		
		if not v.Description then
			print(`No description for { name }.{ k }`)
		end
		
		for _, comment in (if typeof(v.Description) == "table" then v.Description else { v.Description }) do
			if not comment then
				print(`Missing doc comment for '{ k }'`)
			end
		
			datatypes ..= `* { comment }\n`
		end
		
		datatypes ..= "\n"
	end
end

for _, name in orderedLibraries do
	local lib = docComments.ScriptEnv.Libraries[name]
	libraries ..= `### \`{ name }\`\n`
	
	assert(lib, name)
	
	for _, d in (if typeof(lib.Description) == "table" then lib.Description else { lib.Description }) do
		libraries ..= `* { d }\n`
	end
	
	libraries ..= "\n" .. dumpLibMembers(lib.Members, apiDump.ScriptEnv.Libraries[name], name)
end

for _, name in orderedGlobals do
	local contents = docComments.ScriptEnv.Globals[name]
	
	if not contents then
		print(`Missing doc entry for global '{ name }'`)
		continue
	end
	
	if apiDump.ScriptEnv.Globals[name] == "function" then
		local args = contents.In or "..."
		local returns = contents.Out or "..."
		
		globals ..= `#### \`{ name }({ args }): { returns }\`\n`
	else
		globals ..= `#### \`{ name }: { apiDump.ScriptEnv.Globals[name] }\`\n`
	end
	
	for _, comment in (if typeof(contents.Description) == "table" then contents.Description else { contents.Description }) do
		if not comment then
			print(`Missing doc comment for global '{ name }'`)
		end
		
		globals ..= `* { comment }\n`
	end
	
	globals ..= "\n"
end

local rtol = {
	Boolean = "boolean",
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
	local hasFuncs = t.Methods and (count(t.Methods) > 0)
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
				
				s ..= `* \`{ n }: { rtolsub(v) }\`: { d.Properties[n] }\n`
			end
		end
		
		if hasFuncs then
			s ..= "\n### Methods:\n"
			
			local orderedFuncs = keysOf(t.Methods)
			table.sort(orderedFuncs)
			
			for _, n in orderedFuncs do
				local v = t.Methods[n]
				
				if not d.Methods or not d.Methods[n] then
					print(`Missing doc comment for Method '{ cn }:{ n }'`)
					continue
				end
				
				s ..= `* \`{ n }{ rtolsub(v) }\`: { d.Methods[n] }\n`
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

local scriptenv = datatypesPrologue .. datatypes .. librariesPrologue .. libraries .. globalsPrologue .. globals
fs.write(BASE_PATH .. "/scriptenv.txt", scriptenv, false)

local orderedComponents = keysOf(apiDump.GameObject.Components)
table.sort(orderedComponents)
table.insert(orderedComponents, 1, "GameObject")

for _, k in orderedComponents do
	local v = apiDump.GameObject.Components[k]
	local vd = docComments.GameObject.Components[k]
	
	if k == "GameObject" then
		v = apiDump.GameObject.Base
		vd = docComments.GameObject.Base
	end
	
	components ..= `## \`{ k }\`\n\n`
	
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
	
	components ..= dumpReflectionApi(v, vd, k)
end

fs.write(BASE_PATH .. "/components.txt", components, false)

print("Docs CI finish\n\n\n\n\n")

-- close when we're done
engine.exit()

