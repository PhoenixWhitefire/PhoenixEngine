-- 12/12/2024
-- Shader Graph -> GLSL experiment

local ShaderBuilder = require("scripts/modules/ShaderBuilder.luau") :: any

local Graph = {
    ConstVariables = {
        Frag_ColorTint = {
            Type = "Vector3",
            --Value = { 0, 1, 0 }
        }
    },

    RootNodes = {
        {
            Name = "Frag_ColorTint",
            Class = "Variable",
            Outputs = {
                {
                    Name = "FinalOutput",
                    Class = "Vector4"
                }
            }
        }
    }
}

local file_write_rcd = getfenv().file_write_rcd or function(Path, Content, PrependResDir)
    error("Must be run in Phoenix Engine")
end

local Source = ShaderBuilder.BuildSource(Graph)
file_write_rcd("shaders/transgraphs/test1.frag", Source, true)
