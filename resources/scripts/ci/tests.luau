-- tests.luau, 13/08/2025
-- Simple testing engine

local testEntries = fs_listdir("scripts/ci/tests")

local tests = {}
local curGroup = nil
local testCount = 0

local expectationMt = {}
expectationMt.__index = expectationMt
function expectationMt:toBe(v)
	assert(v == self._val, `FAILTEST:Value was '{self._val}' ({ typeof(self._val) }) and not '{v}' ({typeof(v)})`)
end
function expectationMt:toBeNil(v)
	return self:toBe(nil)
end
function expectationMt:toThrow(errstr)
	assert(typeof(self._val) == "function", `':toThrow({ errstr })' called on type { typeof(self._val) }, expected function`)
	
	local success, err = pcall(self._val)
	assert(not success, `FAILTEST:Function didn't throw`)
	
	if errstr then
		assert(string.find(err, errstr), `FAILTEST:Error "{ err }" did not contain string "{ errstr }"`)
	end
	
	return err
end

local function argsToString(...)
	local str = ""
	
	for _, a in {...} do
		if typeof(a) == "string" then
			str ..= `"{a}", `
		else
			str ..= tostring(a) .. ", "
		end
	end
	
	str = string.sub(str, 1, #str - 2)
	return str
end

local expectNeverMt = {}
expectNeverMt.__index = function(self, check)
	assert(expectationMt[check], `invalid index {check}`)
	
	return function(_, ...)
		local success, err = pcall(function(...)
			return expectationMt[check](self, ...)
		end, ...)
		
		if success then
			local errstr = `FAILTEST: Check ':{check}({argsToString(...)})' on value '{self._val}' ({typeof(self._val)}) passed when in '.never' predicate`
			if err then
				errstr ..= `. The thrown error was: { err }`
			end
			error(errstr)
		else
			assert(err:find("FAILTEST:"), err)
			-- the test SHOULD have failed for `:never` to succeed
		end
	end
end

local function describe(group, callback)
	local prevGroup = curGroup
	curGroup[group] = {}
	curGroup = curGroup[group]
	
	callback()
	
	curGroup = prevGroup
end

local function test(description, callback)
	table.insert(curGroup, { IsTest = true, Callback = callback, Description = description })
	testCount += 1
end

local it = test

local function expect(val)
	return setmetatable(
		{
			_val = val,
			never = setmetatable({ _val = val }, expectNeverMt)
		},
		expectationMt
	)
end

local function discoverTests(t, parent)
	for k, v in t do
		local groupName = string.sub(k, string.find(k, "tests/") + #("tests/"), #k)
		parent[groupName] = {}
		curGroup = parent[groupName]
		
		if v == "f" then
			local m = require("./" .. k:sub(k:find("tests/"), #k))
			m(describe, it, expect)
			
		elseif v == "d" then
			discoverTests(fs_listdir(k), curGroup)
		end
	end
end

discoverTests(testEntries, tests)

print(`\n\n\n\nRunning { testCount } tests...\n\n\n`)

local succeeded = 0
local failed = 0

local function runTests(t, depth)
	for index, entry in t do
		if entry.IsTest then
			local success, err = pcall(function()
				entry.Callback()
			end)
			
			if success then
				succeeded += 1
				print(`{ string.rep("    ", depth) }Test {index}: {entry.Description} succeeded`)
			else
				local failReasonStartPos = err:find("FAILTEST:")
				
				if failReasonStartPos then
					print(`{ string.rep("    ", depth) }Test {index}: {entry.Description} FAILED: {string.sub(err, failReasonStartPos + #("FAILTEST:"), #err)}`)
					failed += 1
				else
					error(`TESTING ERROR: A test or the framework threw an unexpected error\n`
						.. `Test {index}: "{entry.Description}"\n`
						.. `Error:\n{err}`)
				end
			end
		else
			print(`{string.rep("    ", depth)}{index}:`)
			runTests(entry, depth + 1)
		end
	end
end

runTests(tests, 0)

print(`{succeeded} succeeded, {failed} failed`)

if succeeded == testCount then
	print("ALL TESTS PASSED") -- :)
	
elseif succeeded == 0 and testCount > 0 then
	print("ALL TESTS FAILED") -- :(
	
elseif testCount == 0 then
	print("No tests :/")
end

print("\n\n\n\n")

game:Close()

