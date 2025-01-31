local ShaderBuilder = require("scripts/modules/ShaderBuilder.luau")

local Links = {}
local LinkIdToToken = {}
local Nodes = { { Class = "Output", Inputs = { { Name = "FinalOutput", DefaultValue = { 1, 1, 1, 1 } } } } }
local ConnectedInputs = {}

local OutputNodes = {
	UVec3 = function()
		table.insert(Nodes, { Class = "UVec3", Inputs = {}, Outputs = { "X", "Y", "Z" }, Attributes = { { Name = "Uniform", Value = "" } } })
	end,
	KVec3 = function()
		table.insert(Nodes, { Class = "KVec3", Inputs = {}, Outputs = { "X", "Y", "Z" }, Attributes = { { Name = "X", Value = 0 }, { Name = "Y", Value = 0}, { Name = "Z", Value = 0 } } })
	end
}

local NodeClassShdGraphHandlers = {
	UVec3 = function(Graph, Me)
		Graph.ConstVariables[Me.Attributes[1].Value] = { Type = "Vector3" }
		
		table.insert(Graph.RootNodes, { Name = Me.Attributes[1].Value, Class = "Variable", Outputs = { { Name = "FinalOutput", Class = "Vector4" } } })
	end,
	KVec3 = function(Graph, Me)
		table.insert(Graph.ConstVariables, { Type = "Vector3", Value = { Me.Attributes[1].Value, Me.Attributes[2].Value, Me.Attributes[3].Value } })
	end,
	Output = function()
		-- no behavior
	end
}

local VectorQtyKeys = { "R", "G", "B", "A" }

local function convertToShaderGraph()
	local graph = {
		ConstVariables = {},
		RootNodes = {}
	}
	
	for _, node in Nodes do
		local handler = NodeClassShdGraphHandlers[node.Class]
		assert(handler, `Unhandled node class '{node.Class}'`)
		
		handler(graph, node)
	end
	
	return graph
end

local function handleInputUI(Name, Value, CreatePins)
	local vt = typeof(Value)
	
	if vt == "number" then
		Value = imgui_input_float(Name, Value)
		
	elseif vt == "string" then
		Value = imgui_input_text(Name, Value)
		
	elseif vt == "table" then
		assert((#Value > 0) and (#Value <= 4), `Expected table value to be within 1-4, was {#Value} instead`)
		
		imgui_text(Name .. ":")
		imgui_indent(25)
		
		for i, v in Value do
			local labelName = VectorQtyKeys[i]
			assert(labelName, `Invalid index into VectorQtyKeys: '{i}'`)
			
			Value[i] = imgui_input_float(labelName, v)
		end
		
		imgui_indent(-25)
	else
		error(`Unrecognized type '{vt}'`)
	end
	
	return Value
end

local function makeInputs(Name, Value, Id)
	local vt = typeof(Value)
	
	if vt == "number" then
		if not ConnectedInputs[Id] then
			imnodes_node_input_begin(Id)
			Value = imgui_input_float(Name, Value)
			imnodes_node_input_end()
		else
			imgui_text(Name)
		end
		
	elseif vt == "string" then
		if not ConnectedInputs[Id] then
			imnodes_node_input_begin(Id)
			Value = imgui_input_text(Name, Value)
			imnodes_node_input_end()
		else
			imgui_text(Name)
		end
		
	elseif vt == "table" then
		assert((#Value > 0) and (#Value <= 4), `Expected table value to be within 1-4, was {#Value} instead`)
		
		imgui_text(Name .. ":")
		imgui_indent(25)
		
		for i, v in Value do
			local vid = Id + i + 1
			imnodes_node_input_begin(vid)
			
			local labelName = VectorQtyKeys[i]
			assert(labelName, `Invalid index into VectorQtyKeys: '{i}'`)
			
			if ConnectedInputs[vid] then
				imgui_text(labelName)
			else
				Value[i] = imgui_input_float(labelName, v)
			end
			
			imnodes_node_input_end()
		end
		
		imgui_indent(-25)
	else
		error(`Unrecognized type '{vt}'`)
	end
	
	return Value
end

return function()
	imgui_begin("Insert new node")
	
	for name, handler in OutputNodes do
		local insert = imgui_button(name)
		if insert then
			handler()
		end
	end
	
	imgui_end()
	
	imnodes_editor_begin()
	
	for index, node in Nodes do
		imnodes_node_begin(index)
		
		for aindex, attr in (node.Attributes or {}) do
			attr.Value = handleInputUI(attr.Name, attr.Value)
		end
		
		for iindex, input in (node.Inputs or {}) do
			local inputId = index + iindex * 100
			input.DefaultValue = makeInputs(input.Name, input.DefaultValue, inputId)
			LinkIdToToken[inputId] = input
		end
		
		for oindex, output in (node.Outputs or {}) do
			imnodes_node_output_begin(index + oindex * 100)
			imgui_text(output)
			imnodes_node_output_end()
		end
		
		imnodes_node_end(index)
	end
	
	for i, os in Links do
		for oi, o in os do
			imnodes_drawlink(i + oi, i, o)
		end
	end
	
	if imgui_button("Compile") then
		local graph = convertToShaderGraph()
		local source = ShaderBuilder.BuildSource(graph)
		file_write_rcd("shaders/transgraphs/test2.frag", source, true)
	end
	
	imnodes_editor_end()
	
	local linkinput, linkendput = imnodes_userlinked()
	
	if linkinput ~= 0 then
		if ConnectedInputs[linkendput] then
			for i, targets in Links do
				for oi, o in targets do
					if o == linkendput then
						table.remove(targets, oi)
					end
				end
			end
		end
		
		Links[linkinput] = Links[linkinput] or {}
		table.insert(Links[linkinput], linkendput)
		ConnectedInputs[linkendput] = true
	end
end