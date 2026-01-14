return function(describe, test, expect)
	test("Encode string", function()
		expect(json.encode("Hello")):toBe('"Hello"')
	end)
	test("Decode string", function()
		expect(json.parse('"Hello"')):toBe("Hello")
	end)
	test("Encode vector", function()
		expect(json.encode(vector.one)):toBe([[{
  "__HX_EncodedData": {
    "data": [
      1.0,
      1.0,
      1.0
    ],
    "type": "vector"
  }
}]])
	end)
	test("Decode vector", function()
		expect(json.parse(json.encode(vector.one))):toBe(vector.one)
	end)

	test("Reserved table key", function()
		expect(function()
			json.encode({ __HX_EncodedData = true })
		end):toThrow("is reserved")
	end)

	test("Encode array", function()
		expect(json.encode({ 1, 2, 3 }, 0)):toBe("[\n1.0,\n2.0,\n3.0\n]")
	end)
	test("Decode array", function()
		expect(json.parse("[ 1, 2, 3 ]")):toBe({ 1, 2, 3 })
	end)
	
	test("Zero-indexing", function()
		expect(function()
			json.encode({ [0] = "hello", [1] = "bye" })
		end):toThrow()
	end)
	test("Negative indices", function()
		expect(function()
			json.encode({ [-3] = 5, [-2] = 42, [-1] = 5.7 })
		end):toThrow()
	end)
	test("Mixed keys", function()
		expect(function()
			json.encode({ [1] = 1, hi = true, [2] = 42 })
		end):toThrow()
	end)
	test("Key types", function()
		expect(function()
			json.encode({ [vector.one] = "bYE" })
		end):toThrow()
	end)
	test("Value types", function()
		expect(function()
			json.encode({ GameObject.new("Example"), vector.zero })
		end):toThrow()
	end)
end


