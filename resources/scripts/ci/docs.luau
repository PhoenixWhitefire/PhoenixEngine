-- docs-ci.luau, 29/04/2025

print("Docs CI begin\n\n\n\n\n")

local Engine: Engine = game:GetService("Engine") :: any

task.delay(5, function()
	warn("Docgen appears to have hung - exiting")
	Engine:Exit(1)
end)

local BASE_PATH = "./wikigen"
local DOCS_PREFIX = "phoenix"
local WIKI_PATH = ".github/wiki"

local apiDump = json.parse(assert(fs.read("./apidump.json"))) :: { [string]: any }
local docComments = json.parse(assert(fs.read(BASE_PATH .. "/doc-comments.json"))) :: { [string]: any }

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
export type EventSignal<T...> = {
	Connect: (self: EventSignal<T...>, Callback: (T...) -> ()) -> (EventConnection),
	WaitUntil: (self: EventSignal<T...>) -> (T...)
}

declare extern type KeyInputEvent with
	Key: EnumKey
	Scancode: number
	Action: EnumInputAction
	Modifiers: number
end

declare extern type MouseButtonInputEvent with
	Button: EnumMouseButton
	Action: EnumInputAction
	Modifiers: number
end

declare extern type ScrollInputEvent with
	X: number
	Y: number
end

export type JsonSerializable = nil | boolean | number | string | vector | { JsonSerializable } | { [string]: JsonSerializable }

declare extern type Enum with
end

declare extern type EnumItem with
end
]]
local documentationData = {}

local GameObjectOverrides = {
	new = "(<T>(ComponentName: T | keyof<Creatables>) -> GameObject & index<Creatables, T>)",
	FindChildWithComponent = "<T>(self, ComponentName: keyof<Creatables> & T) -> (GameObject & index<Creatables, T>)?",
}

local function dumpCreatables()
	local creatablesExtra = {
		Mesh = " & Transform",
		SpotLight = " & Transform",
		PointLight = " & Transform",
		Model = " & Transform",
		ParticleEmitter = " & Transform",
		Camera = " & Transform"
	}

	definitionsFile ..= "\ntype Creatables = {\n"

	for _, v in GameObject.validComponents do
		definitionsFile ..= `\t{v}: {v}{if creatablesExtra[v] then creatablesExtra[v] else ""},\n`
	end

	definitionsFile ..= "}"
end

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

	local lspDef = `({generics}( { defargs } ) -> ({ returns }))`

	if libname == "GameObject" then
		lspDef = GameObjectOverrides[funcname] or lspDef
	end

	return `#### \`{libname}.{ funcname }{generics}({ docargs }): { returns }\`\n`, lspDef
end

local function dumpLibMembers(libm: { [string]: any }, t: { [string]: any }, libname: string)
	if not t then
		return ""
	end

	definitionsFile ..= `declare { libname }: \{\n,\n`

	local str = ""
	local first = true

	if libname == "debug" then
		definitionsFile = string.sub(definitionsFile, 1, #definitionsFile - 2)
		first = false

		definitionsFile ..= "\ttraceback: ((msg: string?, level: number?) -> string) & (co: thread, msg: string?, level: number?) -> string,\n"
							.. "\tinfo: (level: number, s: string) -> string,\n"
	end

	local orderedMembers = keysOf(t)
	table.sort(orderedMembers)

	for _, k in orderedMembers do
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

			if not v.Type then
				assert(not v.Overloads or typeof(v.Overloads) == "string" or typeof(v.Overloads) == "table")

				local ins = { v.In or "" }
				if typeof(v.Overloads) == "string" then
					table.insert(ins, v.Overloads)

				elseif v.Overloads then
					assert(typeof(v.Overloads) == "table")
					table.move(v.Overloads, 1, #v.Overloads, #ins + 1, ins)
				end

				for _, vIn in ins do
					local docsig, defsig = dumpFuncSignature(libname, k, vIn, v.Out or "", v)
					str ..= docsig
					definitionsFile ..= `{ defsig } & `
				end

				definitionsFile = string.sub(definitionsFile, 1, #definitionsFile - 3)
				definitionsFile ..= ",\n"
			else
				assert(typeof(v.Type) == "string")
				str ..= `#### {libname}.{k}{v.Type}`
				definitionsFile ..= v.Type .. ",\n"
			end
		else
			if not v.Type or typeof(v.Type) ~= "string" then
				print("Error with member", k)
				return str
			end
			str ..= `#### \`{libname}.{ k }: { v.Type }\`\n`
			definitionsFile ..= `  { k }: { v.Type },\n`
		end

		local desc = v.Description
		if not desc and typeof(v) == "string" then
			desc = v
		end

		local totalcomment = ""

		for _, comment in (if typeof(desc) == "table" then desc else { desc }) do
			if not comment then
				print(`Missing doc comment for '{ k }'`)
			end

			totalcomment ..= `* { comment }\n`
		end
		totalcomment = totalcomment:sub(1, #totalcomment - 1)

		documentationData[`@{DOCS_PREFIX}/global/{libname}.{k}`] = {
			documentation = totalcomment
		}

		str ..= totalcomment
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
	Vector3 = "vector",
	Vector2 = "vector",
	["->"] = ":",
	READONLY = "",
	RUNTIMEONLY = "",
	Any = "unknown"
}

local function rtolsub(str)
	for p, s in rtol do
		str = string.gsub(str, p, s)
	end

	-- to avoid the `->` being converted to a `:` (which, for function type declarations, is wrong)
	str = string.gsub(str, "Function", "(...unknown) -> (...unknown)")

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

				local luautype = rtolsub(v)
				local docs: any = d.Properties[n]
				local desc = docs

				if typeof(docs) == "table" then
					luautype = docs.Type or luautype
					desc = docs.Description or desc
				end

				if luautype:find("Array") or luautype:find("Map") then
					warn(`Type '{luautype}' has Array or Map, for {cn}.{n}`)
				end

				s ..= `* \`{ n }: { luautype }\`: { desc }\n`
				documentationData[`@{DOCS_PREFIX}/globaltype/{cn}.{n}`] = {
					documentation = desc
				}

				definitionsFile ..= `  { n }: { luautype },\n`
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

				local subst = rtolsub(v)
				local docs: any = d.Methods[n]
				local desc = docs

				if typeof(docs) == "table" then
					subst = docs.Type or subst
					desc = docs.Description or desc

					if docs.Out then
						assert(not docs.Type, `{cn}.{n} has both a .Out and a .Type`)

						subst = subst:sub(1, assert((subst:find(":"))))
						subst ..= ` {docs.Out}`
					end

					if docs.In then
						assert(not docs.Type, `{cn}.{n} has both a .In and a .Type`)
						assert(typeof(docs.In) == "string")

						subst = subst:sub(assert((subst:find(":"))) - 1, #subst)
						subst = " (" .. docs.In .. ")" .. subst
					end
				end

				s ..= `* \`{ n }{ subst }\`: { desc }\n`
				documentationData[`@{DOCS_PREFIX}/globaltype/{cn}.{n}`] = {
					documentation = desc
				}

				subst = string.sub(subst, 2, #subst)
				local close = string.find(subst, ") :")
				assert(close, `{cn}.{n} improper formatting of type, missing ') :'. Got {subst}`)

				if string.sub(subst, 1, 1) == ")" then -- ") : ([...])" when no inputs
					if typeof(docs) == "table" and docs.Out and typeof(docs.Out) == "string" then
						subst = "(self) : " .. docs.Out
					else
						subst = "(self)" .. string.sub(subst, close + 1, #subst)
					end
				elseif ((docs.Type and docs.ParameterNames) or not docs.Type) and not docs.In then
					local inputsstr = string.sub(subst, 1, close)
					local inputs = string.split(inputsstr, ", ")
					local newinputs = {}

					if typeof(docs) == "string" or not docs.ParameterNames then
						warn(`No parameter names for {cn}.{n}`)
					end

					for i, t in inputs do
						if typeof(docs) == "string" or not docs.ParameterNames then
							table.insert(newinputs, `Param_{i}: {t}`)
						else
							local name = if typeof(docs.ParameterNames) == "table" then docs.ParameterNames[i] else docs.ParameterNames
							if not name then
								warn(`Missing parameter name for parameter {i} of {cn}.{n}`)
								name = `Param_{i}`
							end

							table.insert(newinputs, `{name}: {t}`)
						end
					end

					subst = string.sub(subst, close + 1, #subst)

					if typeof(docs) == "table" and docs.Out and typeof(docs.Out) == "string" then
						subst = " : " ..  docs.Out
					end

					subst = table.concat(newinputs, ", ") .. subst
					
					subst = "(self, " .. subst
				elseif not docs.In then
					assert(docs.Type, n)
					if not docs.Type:find("self") then
						warn(`In {cn}.{n}, 'self' must be the first parameter!`)
					end

					subst = docs.Type
				elseif docs.In then
					subst = `(self, ` .. subst:sub(2, #subst)
				end

				subst = GameObjectOverrides[n] or subst
				subst = string.gsub(subst, " : ", " -> ")
				subst = string.gsub(subst, "self", `self: {cn}`)

				if n ~= "SetShaderVariable" then
					definitionsFile ..= `  { n }: { subst },\n`
				else
					definitionsFile ..= "  SetShaderVariable: ((_: Renderer, Shader: string, Variable: string, Value: boolean | number | Color | Matrix, Type: EnumValueType?) -> ()) & ((_: Renderer, Shader: string, Variable: string, Value: vector, Type: EnumValueType) -> ()),\n"
				end

				if subst:find("Array") or subst:find("Map") then
					warn(`Type '{subst}' has Array or Map, for {cn}.{n}`)
				end
			end
		end

		if hasEvnts then
			definitionsFile ..= "\n"
			s..= "\n### Events:\n"
			
			local orderedEvents = keysOf(t.Events)
			table.sort(orderedEvents)

			for _, n in orderedEvents do
				local v = t.Events[n]

				if not d.Events or not d.Events[n] or not d.Events[n].Description then
					print(`Missing doc comment for Event '{ cn }.{ n }'`)
					continue
				end

				s ..= `* \`{ n }{ rtolsub(v) }\`: { d.Events[n].Description }\n`
				documentationData[`@{DOCS_PREFIX}/globaltype/{cn}.{n}`] = {
					documentation = d.Events[n].Description
				}
				definitionsFile ..= `  { n }: EventSignal<{d.Events[n].ArgumentTypes or rtolsub(v)}>,\n`
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
		if not definitionsFile:find(`declare extern type { t } with`) then
			documentDatatype(t)
		end
	end

	datatypes ..= `### \`{ name }\`\n\n`

	local totalcomment = ""
	for _, d in (if typeof(dt.Description) == "table" then dt.Description else { dt.Description }) do
		totalcomment ..= `* { d }\n`
	end
	totalcomment = totalcomment:sub(1, #totalcomment - 1)

	datatypes ..= totalcomment
	documentationData[`@{DOCS_PREFIX}/global/{name}`] = {
		documentation = totalcomment
	}

	local datatype = ""

	if name ~= "GameObject" and name ~= "EventSignal" then
		definitionsFile ..= `declare extern type { name } with\n`
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

	local orderedMembers = keysOf(dt.Members)
	table.sort(orderedMembers)

	for _, k in orderedMembers do
		local v = dt.Members[k]

		assert(k and typeof(k) == "string")
		if string.sub(k, 1, 1) == ":" then
			local rk = string.sub(k, 2, #k)
			k = rk
			local args = v.In or ""
			local returns = v.Out or nil
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

				if not args:find(":") then
					defargs = "self, " .. table.concat(newinputs, ", ")
				else
					defargs = "self, " .. args
				end
			end

			datatype ..= `#### \`{ rk }({ args }){ if returns then ": " .. returns else "" }\`\n`

			if name ~= "EventSignal" then
				definitionsFile ..= `  function { rk }({ defargs }): ( { returns or "" } )\n`
			end
		else
			datatype ..= `#### \`.{ k }: { v.Type }\`\n`

			if name ~= "EventSignal" then
				definitionsFile ..= `  { k }: { v.Type }`
			end

			if k == "SceneCamera" then
				definitionsFile ..= "& Camera"
			end

			definitionsFile ..= "\n"
		end

		if not v.Description then
			print(`No description for { name }.{ k }`)
		end

		local totalcomment = ""
		for _, comment in (if typeof(v.Description) == "table" then v.Description else { v.Description }) do
			if not comment then
				print(`Missing doc comment for '{ k }'`)
			end

			totalcomment ..= `* { comment }\n`
		end
		totalcomment = totalcomment:sub(1, #totalcomment)

		datatype ..= totalcomment
		documentationData[`@{DOCS_PREFIX}/globaltype/{name}.{k}`] = {
			documentation = totalcomment
		}

		datatype ..= "\n"
	end

	if name ~= "GameObject" and name ~= "EventSignal" then
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

	-- intersections don't work with extern types
	definitionsFile ..= `export type { k } = \{\n`

	components ..= `## \`{ k }\`\n\n`

	if not vd then
		print(`No docs for Component '{ k }'`)
		definitionsFile ..= "end\n"
		continue
	end

	if vd.Description then
		for _, d in (if typeof(vd.Description) == "table" then vd.Description else { vd.Description }) do
			components ..= `* { d }\n`
		end

		components ..= "\n"
	end

	components ..= dumpReflectionApi(v, vd, k)

	definitionsFile ..= "}\n"
end

definitionsFile ..= "\n\n"

dumpCreatables()

definitionsFile ..= "\n"

for _, name in orderedLibraries do
	local lib = docComments.ScriptEnv.Libraries[name]
	libraries ..= `### \`{ name }\`\n`

	assert(lib, name)

	local totalcomment = ""
	for _, d in (if typeof(lib.Description) == "table" then lib.Description else { lib.Description }) do
		totalcomment ..= `* { d }\n`
	end
	totalcomment = totalcomment:sub(1, #totalcomment - 1)

	libraries ..= totalcomment
	documentationData[`@{DOCS_PREFIX}/global/{name}`] = {
		documentation = totalcomment
	}

	libraries ..= "\n"

	if name ~= "Enum" then
		libraries ..= dumpLibMembers(lib.Members, apiDump.ScriptEnv.Libraries[name], name)
	else
		-- thank you johnny
		-- https://github.com/JohnnyMorganz/luau-lsp/blob/b700f428a23779c43be96e09cb999b16604289d5/scripts/globalTypes.d.luau#L59

		local members = lib.Members
		local enumList = "declare Enum: {\n"
		local namesUnion = ""
		local typesUnion = ""

		local orderedMemberNames = keysOf(Enum)
		table.sort(orderedMemberNames)

		for _, e in orderedMemberNames do
			local n = Enum[e]

			definitionsFile ..= `declare extern type Enum{e} extends EnumItem with end\n`
			definitionsFile ..= `declare extern type Enum{e}_INTERNAL extends Enum with\n`
			namesUnion ..= ` | "{e}"`
			typesUnion ..= ` | Enum{e}_INTERNAL`

			assert(members[e], `Enum.{e} has no documentation`)

			local totalcomment = ""

			for _, comment in (if typeof(members[e].Description) == "table" then members[e].Description else { members[e].Description }) do
				if not comment then
					print(`Missing doc comment for Enum.{e}`)
				end

				totalcomment ..= `* { comment }\n`
			end
			totalcomment = totalcomment:sub(1, #totalcomment - 1)

			local enumDocs = {
				documentation = totalcomment,
				keys = {}
			}
			documentationData[`@{DOCS_PREFIX}/global/Enum.{e}`] = enumDocs

			if not members[e].Items then
				warn(`Enum.{e} has no documentation for its items`)
			end

			local orderedItems = keysOf(n)
			table.sort(orderedItems)

			for _, k in orderedItems do
				definitionsFile ..= `  {k}: Enum{e}\n`
				if members[e].Items and not members[e].Items[k] then
					warn(`Enum.{e}.{k} has no documentation`)
				end

				enumDocs.keys[k] = `@{DOCS_PREFIX}/enum/{e}.{k}`
				documentationData[`@{DOCS_PREFIX}/enum/{e}.{k}`] = {
					documentation = (members[e].Items or {})[k] or ""
				}
			end

			definitionsFile ..= "end\n"
			enumList ..= `  {e}: Enum{e}_INTERNAL,\n`
		end

		enumList ..= `  [{namesUnion}]: {typesUnion}\n}\n\n`
		definitionsFile ..= enumList
	end
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

	local totalcomment = ""
	for _, comment in (if typeof(contents.Description) == "table" then contents.Description else { contents.Description }) do
		if not comment then
			print(`Missing doc comment for global '{ name }'`)
		end

		totalcomment ..= `* { comment }\n`
	end
	totalcomment = totalcomment:sub(1, #totalcomment - 1)

	globals ..= totalcomment
	documentationData[`@{DOCS_PREFIX}/global/{name}`] = {
		documentation = totalcomment
	}

	globals ..= "\n"
end

definitionsFile ..= "\n\n"

local scriptenv = datatypesPrologue .. datatypes .. librariesPrologue .. libraries .. globalsPrologue .. globals

fs.write(WIKI_PATH .. "/API-‐-Script-Environment.md", scriptenv)
fs.write(WIKI_PATH .. "/API-‐-Components.md", components)

fs.write("./lsp/api.d.luau", definitionsFile)
fs.write("./lsp/api-docs.json", json.encode(documentationData))

print("Docs CI finish\n\n\n\n\n")

if not _G.LspDefinitionsGeneratorDoNotShutdown then
	-- close when we're done
	Engine:Exit()
end
