local Types = require("@ed/Types.luau")

local TestPlugin: Types.Plugin = {
    Name = "Niko Plugin",
    Path = "",
    Description = "I love pancakes",
    Icon = "@editres/textures/niko.png",
    ActiveStages = { "ProjectEditing" },
    ToolbarButtons = {}
}

table.insert(TestPlugin.ToolbarButtons, {
    Text = "Say Hello",
    Enabled = true,
    OnClick = function(Button: Types.ToolbarButton)
        Button.Enabled = false
        print("Hello!")

        defer(function()
            Button.Enabled = true
            Button.Lit = true
        end, 1)
    end,
    Icon = "@editres/textures/niko.png"
})

function TestPlugin:OnEditorStageChanged(OldStage, NewStage)
    print(`Niko is travelling through the stages: { OldStage } -> { NewStage }`)
end

function TestPlugin:OnLoad()
    print("Niko loaded")
end

function TestPlugin:OnShutdown()
    print("Niko says goodbye!")
end

local totalRunningTime = 0

function TestPlugin:OnUpdate(dt: number)
    totalRunningTime += dt

    if totalRunningTime > 10 then
        print("Niko: Such productivity")
        totalRunningTime = -math.huge
    end
end

print("Niko joined the party")

return TestPlugin
