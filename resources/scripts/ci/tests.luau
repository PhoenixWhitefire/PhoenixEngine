-- tests.luau, 13/08/2025
-- Simple testing engine

-- Sleep for a little bit to avoid tests appearing to take several milliseconds due to initial slowdowns
task.wait(0.04)

task.delay(15, function()
	warn("Tests appear to have hung - exiting")
	game:GetService("Engine"):Exit(1)
end)

local start = os.clock()

local Types = require("./test-types")

local testEntries = fs.listdir("scripts/ci/tests")

type TestsNode = {
	IsTest: true,
	Name: string,
	Callback: () -> ()
} | { IsTest: nil, [number]: TestsNode }

local tests: TestsNode = { IsTest = nil }
local curGroup: TestsNode? = nil
local testCount = 0

local function contextify(v)
	if typeof(v) == "string" then
		return `.{v}`
	else
		return `[{v}]`
	end
end

local function testAreDeepEqual(selfv: unknown, expectedv: unknown, context: string?)
	context = context or ""
	
	if typeof(selfv) ~= typeof(expectedv) or typeof(selfv) ~= "table" then
		assert(selfv == expectedv, `FAILTEST:Value{context} was '{selfv}' ({ typeof(selfv) }) and not '{expectedv}' ({typeof(expectedv)})`)
	else
		assert(typeof(selfv) == "table")
		assert(typeof(expectedv) == "table")

		for i, v in expectedv do
			testAreDeepEqual(selfv[i], v, context .. contextify(i))
		end
		
		for i, v in selfv do
			testAreDeepEqual(v, expectedv[i], context .. contextify(i))
		end
	end
end

local expectationMt = {}
expectationMt.__index = expectationMt
function expectationMt:toBe(val)
	testAreDeepEqual(self._val, val)
end
function expectationMt.toBeNil(self)
	expectationMt.toBe(self, nil)
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
function expectNeverMt.__index(self, check)
	assert(expectationMt[check], `invalid index {check}`)
	
	return function(_, ...)
		local success, err: string | any = pcall(function(...)
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
			-- the test SHOULD have failed for `.never` to succeed
		end
	end
end

local test: Types.Test = function(name: string, callback)
	assert(curGroup)
	assert(not curGroup.IsTest)

	table.insert(curGroup, { IsTest = true :: true, Callback = callback, Name = name })
	testCount += 1
end

local function expect(val)
	return setmetatable(
		{
			_val = val,
			never = setmetatable({ _val = val }, expectNeverMt)
		},
		expectationMt
	)
end

local function discoverTests(t, parent: TestsNode)
	for k: string, v in t do
		local lastSlash = #k - assert((string.find(string.reverse(k), "/")))
		local groupName = string.sub(k, lastSlash + 2, #k)
		parent[groupName] = { IsTest = nil }
		curGroup = parent[groupName]

		if v == "f" then
			assert(k:find("tests/"), k)
			local m = require("./" .. k:sub(assert((k:find("tests/"))), #k)) :: any
			m(test, expect)

		elseif v == "d" then
			assert(curGroup)
			discoverTests(fs.listdir(k), curGroup)
		end
	end
end

discoverTests(testEntries, tests)

appendlog(`\n\n`)
print(`Running { testCount } tests...\n\n\n`)

local succeeded = 0
local failed = 0
local totaltesttime = 0

local function errorStackTrace(errm)
	return { errm, debug.traceback(errm, 2) }
end

local function runTests(t: TestsNode, depth)
	for index, entry in t do
		if entry.IsTest then
			local indent = string.rep("    ", depth)

			local printstr = `{indent}Test {index}: {entry.Name}... `
			print(printstr, string.rep(" ", 60 - #printstr), "&&")

			local tstart = os.clock()

			local success, errd = (xpcall :: any)(function()
				entry.Callback()
			end, errorStackTrace)

			totaltesttime += os.clock() - tstart
			local mstime = (os.clock() - tstart) * 1000
			local mstimestr = if mstime > 1 then math.ceil(mstime) .. "ms" else "<1ms"

			if success then
				succeeded += 1
				appendlog(`Succeeded ({mstimestr})`)
			else
				local errm = if typeof(errd) == "table" then errd[1] else errd
				local failReasonStartPos = errm:find("FAILTEST:")

				if failReasonStartPos then
					appendlog(`FAILED ({mstimestr}): {string.sub(errm, failReasonStartPos + #("FAILTEST:"), #errd[1])}`)
					failed += 1
				else
					appendlog("\n")
					error(`TESTING ERROR: A test or the framework threw an unexpected error\n`
						.. `Test {index}: "{entry.Name}"\n`
						.. `Error:\n{errm}\n-- End caught exception --`)
				end
			end
		else
			print(`{string.rep("    ", depth)}{index}:`)
			runTests(entry, depth + 1)
		end
	end
end

runTests(tests, 0)

appendlog("\n")
print("Finished testing.")
print(`{succeeded} succeeded, {failed} failed`)

if succeeded == testCount then
	print("ALL TESTS PASSED") -- :)
	
elseif succeeded == 0 and testCount > 0 then
	print("ALL TESTS FAILED") -- :(
	
elseif testCount == 0 then
	print("No tests :/")
end

appendlog("\n\n")
print(`Testing session took { os.clock() - start } seconds. Tests themselves took { totaltesttime } seconds in total`)

appendlog("\n\n\n");

((game:GetService("Engine") :: any) :: Engine):Exit(if failed == 0 then 0 else 1)
