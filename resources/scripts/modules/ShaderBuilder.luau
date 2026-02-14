--!strict

-- This is definitely something 13/12/2024

local ShaderBuilder = {}

type Variable = {
    Type: string,
    Value: any
}

type Node = {
    Name: string,
    Inputs: { Node },
    Outputs: { Node },
    Class: string,
    Attributes: { [string]: Variable }
}

type NodeGraph = {
    ConstVariables: { [string]: Variable },
    RootNodes: { Node }
}

local InitialState, InitialStateError = fs.read("shaders/ShaderGraphInitial.frag")
local FinishCode = "\n\n//END SHADER GRAPH TRANSPILATION\n\n}"

assert(InitialState, InitialStateError)

local VariableDeclarationMap: { [string]: (string, Variable) -> (string) } = {
    Number = function(Name, Variable)
        assert(typeof(Variable.Value) == "number", "Must be a number")
        return `float {Name} = {tonumber(Variable.Value)};`
    end,
    Vector2 = function(Name, Variable)
        assert(typeof(Variable.Value) == "table" and #Variable.Value == 2, "Must be an array with exactly 4 elems")
        return `vec2 {Name} = vec2({tonumber(Variable.Value[1])}, {tonumber(Variable.Value[2])});`
    end,
    Vector3 = function(Name, Variable)
        assert(typeof(Variable.Value) == "table" and #Variable.Value == 3, "Must be an array with exactly 4 elems")
        return `vec3 {Name} = vec3({tonumber(Variable.Value[1])}, {tonumber(Variable.Value[2])}, {tonumber(Variable.Value[3])});`
    end,
    Vector4 = function(Name, Variable)
        assert(typeof(Variable.Value) == "table" and #Variable.Value == 4, "Must be an array with exactly 4 elems")
        return `vec4 {Name} = vec4({tonumber(Variable.Value[1])}, {tonumber(Variable.Value[2])}, {tonumber(Variable.Value[3])}, {tonumber(Variable.Value[4])}));`
    end,
}

local VarCastFromMap: { [string]: { [string]: (string, Variable) -> (string) } } = {
    Vector4 = {
        Vector3 = function(Name, Var)
            return `vec4({Name}, 1.f)`
        end
    }
}

local kvars = {}

local NodeClassHandlers: { [string]: (Node) -> (string) } = {
    -- set all Os to N
    Variable =  function(Node)
        assert(#Node.Outputs > 0, "Node must have atleast 1 Output")
	
        local source = ""
	
        for _, out in Node.Outputs do
            if out.Class == Node.Class then
                source ..= `{out.Name} = {Node.Name};\n`
            else
                local targetCasts = VarCastFromMap[out.Class]
                assert(targetCasts, `No casts available for Class '{out.Class}'`)

                assert(Node.Class == "Variable")
                local var = kvars[Node.Name]

                local castHandler = targetCasts[var.Type]
                assert(castHandler, `No casting for '{var.Type}' to '{out.Class}' defined`)

                source ..= `{out.Name} = {castHandler(Node.Name, var)};`
            end
        end

        return source
    end
}

local function translateNode(Node: Node) : string
    local source = ""

    local handler = NodeClassHandlers[Node.Class]
    assert(handler, `Unable to handle a Node of Class '{Node.Class}'`)

    source ..= handler(Node)

    return source
end

local function addConstVariables( Variables: { [string]: Variable } ) : string
    local source = ""

    for name, variable in Variables do
        if (variable.Value == nil) then
            continue -- use default value/shader uniform
        end

        local handler = VariableDeclarationMap[variable.Type]
        assert(handler, `Unable to handle a variable type of '{variable.Type}'`)

        source = `{source}const {handler(name, variable)}\n`
    end

    return source
end

function ShaderBuilder.BuildSource(Graph: NodeGraph) : string
    local source = InitialState
    source ..= "// CONST VARS\n\n" .. addConstVariables(Graph.ConstVariables) .. "\n\n"
    source ..= "// NODES\n\n"

    kvars = Graph.ConstVariables

    for _, rootNode in Graph.RootNodes do
        source ..= translateNode(rootNode) .. "\n";
    end

    source ..= FinishCode

    return source
end

return ShaderBuilder
