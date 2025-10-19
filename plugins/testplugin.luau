local TestPlugin = {}
TestPlugin.Name = "Test Plugin"
TestPlugin.Description = "I love testing"
TestPlugin.Icon = "textures/address-san.jpg"
TestPlugin.ActiveStages = { "ProjectEditing" }

TestPlugin.ToolbarButtons = {}
table.insert(TestPlugin.ToolbarButtons, {
    Text = "Say Hello",
    Enabled = true,
    OnClick = function(Button: index<typeof(TestPlugin.ToolbarButtons), number>)
        Button.Enabled = false
        print("Hello!")

        defer(function()
            Button.Enabled = true
            Button.Lit = true
        end, 1)
    end,
    Icon = "textures/address-san.jpg"
})

function TestPlugin:OnEditorStageChanged(...)
    print("Test Plugin is travelling through the stages:", ...)
end

function TestPlugin:OnLoad()
    print("Test Plugin loaded")
end

function TestPlugin:OnShutdown()
    print("Test Plugin shutting down")
end

local totalRunningTime = 0

function TestPlugin:OnUpdate(dt: number)
    totalRunningTime += dt

    if totalRunningTime > 10 then
        print("So much productivity")
        totalRunningTime = -math.huge
    end
end

return TestPlugin