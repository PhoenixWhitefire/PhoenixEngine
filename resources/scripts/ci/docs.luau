-- docs-ci.luau, 29/04/2025

print("Docs CI begin\n\n\n\n\n")

local BASE_PATH = "./gh-assets/wiki"

local apiDump = json.parse(assert(fs.read("./apidump.json")))
local docComments = json.parse(assert(fs.read(BASE_PATH .. "/doc-comments.json")))

local datatypesPrologue = fs.read(BASE_PATH .. "/datatypes-prologue.md")
local librariesPrologue = fs.read(BASE_PATH .. "/libraries-prologue.md")
local globalsPrologue = fs.read(BASE_PATH .. "/globals-prologue.md")
local componentsPrologue = fs.read(BASE_PATH .. "/components-prologue.md")

assert(datatypesPrologue)
assert(librariesPrologue)
assert(globalsPrologue)
assert(componentsPrologue)

datatypesPrologue = "## Datatypes\n\n" .. datatypesPrologue
librariesPrologue = "## Libraries\n\n" .. librariesPrologue
globalsPrologue = "## Globals\n\n" .. globalsPrologue

local datatypes = ""
local libraries = ""
local globals = ""
local components = componentsPrologue .. "\n\n"

local function keysOf<T>(t: { [T]: any }) : { T }
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

local definitionsFile = [[
declare class Vector2
end
]]

local function dumpFuncSignature(libname, funcname, In, Out, V)
	local docargs = In or ""
	local returns = Out or ""
	
	if typeof(V) == "string" then
		docargs = "" -- inaccurate, overly-lax type definitions are worse than none at all
		returns = ""
	end
	
	local defargs = docargs
	
	if typeof(docargs) == "table" then
		local docasstr = ""
		defargs = ""
		
		for k, a in docargs do
			if typeof(a) == "table" then
				docasstr ..= `{ a[1] } [ {a[2] } ], `
				defargs ..= `{ a[1] }, `
			else
				docasstr ..= `{ a }, `
				defargs ..= `{ a }, `
			end
		end
		
		docasstr = string.sub(docasstr, 1, #docasstr - 2)
		defargs = string.sub(defargs, 1, #defargs - 2)
		docargs = docasstr
	end
	
	if funcname == "new" and not Out then
		returns = libname
	end

	local generics = ""
	if V.Generics then
		generics = `<{V.Generics}>`
	end

	local lspDef = `{generics}(( { defargs } ) -> ({ returns }))`

	-- generates "Code too complex to typecheck" error in definitions file :(
	--[[
	if libname == "GameObject" and funcname == "new" then
		local genericDef = lspDef -- aka the `(Component: string) -> (GameObject)` one
		lspDef = ""

		-- some components will automatically add other components to the Object if they
		-- are not present
		local EXTRA_COMPONENT_INITS = {
			PointLight = { "Transform" },
			DirectionalLight = { "Transform" },
			SpotLight = { "Transform" },
			Mesh = { "Transform" },
			Model = { "Transform" },
			ParticleEmitter = { "Transform" },
		}

		for _, v in GameObject.validComponents do
			local finalComponents = v
			local extra = EXTRA_COMPONENT_INITS[v]

			if extra then
				for _, e in extra do
					finalComponents ..= ` & {e}`
				end
			end

			lspDef ..= `(( Component: "{v}" ) -> ( GameObject & {finalComponents} )) & `
		end

		lspDef ..= genericDef
	end
	]]

	return `#### \`{libname}.{ funcname }{generics}({ docargs }): { returns }\`\n`, lspDef
end

local function dumpLibMembers(libm: { [string]: any }, t: { [string]: any }, libname: string)
	if not t then
		return ""
	end
	
	definitionsFile ..= `declare { libname }: \{\n,\n`
	
	local str = ""
	local first = true
	
	for k, d in t do
		if first then
			definitionsFile = string.sub(definitionsFile, 1, #definitionsFile - 2)
				
			first = false
		end
		
		local v = libm[k]
		
		if not v or not v.Description then
			print(`Missing docs for { libname }.{ k }`)
		end
		
		v = v or {}
		
		if t[k] == "function" then
			definitionsFile ..= `  { k }: `
			
			for _, vIn in { v.In or "", table.unpack(v.Overloads or {}) } do
				local docsig, defsig = dumpFuncSignature(libname, k, vIn, v.Out or "", v)
				str ..= docsig
				definitionsFile ..= `{ defsig } & `
			end
			
			definitionsFile = string.sub(definitionsFile, 1, #definitionsFile - 3)
			definitionsFile ..= ",\n"
		else
			assert(v.Type and typeof(v.Type) == "string", k);
			str ..= `#### \`{libname}.{ k }: { v.Type }\`\n`
			definitionsFile ..= `  { k }: { v.Type },\n`
		end
		
		local desc = v.Description
		if not desc and typeof(v) == "string" then
			desc = v
		end
		
		for _, comment in (if typeof(desc) == "table" then desc else { desc }) do
			if not comment then
				print(`Missing doc comment for '{ k }'`)
			end
		
			str ..= `* { comment }\n`
		end
		
		str ..= "\n"
	end
	
	definitionsFile = string.sub(definitionsFile, 1, #definitionsFile - 2)
	definitionsFile ..= `\n}\n`
	
	return str
end

local rtol: { [string]: string } = {
	Boolean = "boolean",
	Integer = "number",
	Double = "number",
	String = "string",
	Array = "{ any }",
	Map = "{ [string]: any }",
	Vector3 = "vector",
	["->"] = ":",
	READONLY = "",
	RUNTIMEONLY = ""
}

local function rtolsub(str)
	for p, s in rtol do
		str = string.gsub(str, p, s)
	end
	
	-- to avoid the `->` being converted to a `:` (which, for function type declarations, is wrong)
	str = string.gsub(str, "Function", "(any) -> (any)")
	
	return str
end

local function count(t: { any })
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

				if n == "SceneCamera" then
					definitionsFile ..= `  { n }: { "(GameObject & Camera)?" }\n`
				else
					definitionsFile ..= `  { n }: { rtolsub(v) }\n`
				end
			end
		end
		
		if hasFuncs then
			definitionsFile ..= "\n"
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
				
				local subst = rtolsub(v)
				subst = string.sub(subst, 2, #subst)
				
				if string.sub(subst, 1, 1) == ")" then -- ") : ()" when no inputs
					local close = string.find(subst, ") :")
					assert(close)

					subst = "(self)" .. string.sub(subst, close + 1, #subst)

				else
					local close = string.find(subst, ") :")
					assert(close)
					
					local inputsstr = string.sub(subst, 1, close)
					local inputs = string.split(inputsstr, ", ")
					local newinputs = {}
					
					for i, t in inputs do
						table.insert(newinputs, `Param_{i}: {t}`)
					end
					
					subst = string.sub(subst, close + 1, #subst)
					subst = table.concat(newinputs, ", ") .. subst
					
					subst = "(self, " .. subst
				end
				
				definitionsFile ..= `  function { n }{ subst }\n`
			end
		end
		
		if hasEvnts then
			definitionsFile ..= "\n"
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
				definitionsFile ..= `  { n }: EventSignal\n`
			end
		end
	else
		s = "No members defined\n"
	end
	
	s ..= "\n"
	
	return s
end

local alreadyAdded = {}
local descsForMetafields = {
	__type = "Calling <code>typeof</code> on this object will return %s",
	__mul = "You may multiply this object with another object via the <code>*</code> operator",
	__div = "You may divide this object with another object via the <code>/</code> operator",
	__imul = "You may perform integer division on this object with another object via the <code>//</code> operator",
	__sub = "You may substract this object with another object via the <code>-</code> operator",
	__add = "You may add this object with another object via the <code>+</code> operator",
	__index = "You may read a value of this object via the <code>.</code> or <code>[\"\"]</code> index operators",
	__newindex = "You may assign a value of this object via the <code>=</code> operator",
	__metatable = "This object's metatable is locked",
	__tostring = "This object can be converted to a string representation via <code>tostring</code>",
	__namecall = "This object has methods that can be called with the <code>:</code> operator"
}

local function documentDatatype(name)
	if alreadyAdded[name] then
		return
	end
	alreadyAdded[name] = true
	
	local dt = docComments.ScriptEnv.Datatypes[name]
	assert(dt, name)
	
	for _, t in (dt.TypeDependencies or {}) do
		if not definitionsFile:find(`declare class { t }`) then
			documentDatatype(t)
		end
	end
	
	datatypes ..= `### \`{ name }\`\n\n`
	
	for _, d in (if typeof(dt.Description) == "table" then dt.Description else { dt.Description }) do
		datatypes ..= `* { d }\n`
	end
	
	local datatype = ""
	
	if name ~= "GameObject" then
		definitionsFile ..= `declare class { name }\n`
	end
	
	dt.Members = dt.Members or {}
	
	--[[
	for k, v in (apiDump.ScriptEnv.Datatypes[name].Metatable or {}) do
		if not dt.Members[k] then
			local formatString = descsForMetafields[k]
			assert(formatString, k)
			
			if v == "function" then
				local vIn = "...: any"
				if k == "__index" or k == "__newindex" then
					vIn = "string"
				elseif k == "__tostring" then
					vIn = ""
				end
				
				dt.Members[":" .. k] = {
			 		In = vIn,
			 		Out = if k == "__tostring" then "string" else "any",
			 		Description = formatString:format(v)
				}
			else
				dt.Members[k] = {
					Type = typeof(v),
					Description = formatString:format(v)
				}
			end
		end
	end
	]]--
	
	for k, v in dt.Members do
		assert(k and typeof(k) == "string")
		if string.sub(k, 1, 1) == ":" then
			local rk = string.sub(k, 2, #k)
			local args = v.In or ""
			local returns = v.Out or "nil"
			local defargs = args
			
			if #args == 0 then
				defargs = "self"
			else
				assert(args and typeof(args) == "string")
				local inputs = string.split(args, ", ")
				local newinputs = {}
				
				for i, t in inputs do
					table.insert(newinputs, `Param_{i}: {t}`)
				end
				
				defargs = "self, " .. table.concat(newinputs, ", ")
			end
			
			datatype ..= `#### \`{ rk }({ args }): { returns }\`\n`
			definitionsFile ..= `  function { rk }({ defargs }): ( { returns } )\n`
		else
			datatype ..= `#### \`.{ k }: { v.Type }\`\n`
			definitionsFile ..= `  { k }: { v.Type }`

			if k == "SceneCamera" then
				definitionsFile ..= "& Camera"
			end

			definitionsFile ..= "\n"
		end
		
		if not v.Description then
			print(`No description for { name }.{ k }`)
		end
		
		for _, comment in (if typeof(v.Description) == "table" then v.Description else { v.Description }) do
			if not comment then
				print(`Missing doc comment for '{ k }'`)
			end
		
			datatype ..= `* { comment }\n`
		end
		
		datatype ..= "\n"
	end
	
	if name ~= "GameObject" then
		definitionsFile ..= "end\n"
	end
	
	datatypes ..= "\n" .. dumpLibMembers(dt.Library, apiDump.ScriptEnv.Datatypes[name].Library, name)
	datatypes ..= datatype
end

for _, name in orderedDatatypes do
	documentDatatype(name)
end

definitionsFile ..= "\n\n"

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
	
	definitionsFile ..= `declare class { k }\n`
	
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
	
	definitionsFile ..= "end\n"
end

definitionsFile ..= "\n\n"

for _, name in orderedLibraries do
	local lib = docComments.ScriptEnv.Libraries[name]
	libraries ..= `### \`{ name }\`\n`
	
	assert(lib, name)
	
	for _, d in (if typeof(lib.Description) == "table" then lib.Description else { lib.Description }) do
		libraries ..= `* { d }\n`
	end
	
	libraries ..= "\n" .. dumpLibMembers(lib.Members, apiDump.ScriptEnv.Libraries[name], name)
end

definitionsFile ..= "\n\n"

for _, name in orderedGlobals do
	local contents = docComments.ScriptEnv.Globals[name]
	
	if not contents then
		print(`Missing doc entry for global '{ name }'`)
		continue
	end
	
	if apiDump.ScriptEnv.Globals[name] == "function" then
		local args = contents.In or ""
		local returns = contents.Out or "()"

		local generics = ""
		if contents.Generics then
			generics = `<{contents.Generics}>`
		end

		globals ..= `#### \`{ name }{generics}({ args }): { returns }\`\n`
		definitionsFile ..= `declare function { name }{generics}( { args } ): { returns }\n`
	else
		globals ..= `#### \`{ name }: { apiDump.ScriptEnv.Globals[name] }\`\n`
		definitionsFile ..= `declare { name }: { apiDump.ScriptEnv.Globals[name] }\n`
	end
	
	for _, comment in (if typeof(contents.Description) == "table" then contents.Description else { contents.Description }) do
		if not comment then
			print(`Missing doc comment for global '{ name }'`)
		end
		
		globals ..= `* { comment }\n`
	end
	
	globals ..= "\n"
end

definitionsFile ..= "\n\n"

local scriptenv = datatypesPrologue .. datatypes .. librariesPrologue .. libraries .. globalsPrologue .. globals
fs.write(BASE_PATH .. "/scriptenv.md", scriptenv)

fs.write(BASE_PATH .. "/components.md", components)

fs.write("scripts/api.d.luau", definitionsFile)

print("Docs CI finish\n\n\n\n\n")

if not conf.get("LspDefinitionsGeneratorDoNotShutdown") then
	-- close when we're done
	engine.exit()
end
