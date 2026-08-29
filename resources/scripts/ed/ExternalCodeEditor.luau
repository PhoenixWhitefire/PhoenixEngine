-- ExternalCodeEditor.luau, 31/05/2026
local ExternalCodeEditor = {
    RequestOpenSettingsMenuToConfigure = false,
}

local Engine = game:GetService("Engine")

local process = require("@std/process")
-- local json = require("@std/json")
local fs = require("@std/fs")

local EditorSettingsMenu = require("@ed/EditorSettingsMenu")
local ErrorMessage = require("@ed/ErrorMessage")
local Preferences = require("@ed/Preferences")

local json = phoenix.json

function ExternalCodeEditor.Open(ProjectName: string)
    if not Preferences.CodeEditorPath or #Preferences.CodeEditorPath == 0 then
        Engine:ShowMessageBox(
            "Configure Code Editor",
            "You have not configured what Code Editor you would like the Editor to use.\n"
            .. "You can set the command to launch your desired Code Editor in Editor Settings.\n"
            .. "Look up the relevant command for your Code Editor. For example, the command for Visual Studio Code is 'code'."
        )

        ExternalCodeEditor.RequestOpenSettingsMenuToConfigure = true
        EditorSettingsMenu.RequestOpenSection = "ExternalTools"
    else
        ExternalCodeEditor.RequestOpenSettingsMenuToConfigure = false

        if Preferences.CodeEditorPath == "code" or Preferences.CodeEditorPath == "codium" then
            local folderPath = `@project/.vs{ Preferences.CodeEditorPath }`

            if fs.type(folderPath) ~= "dir" then
                local lsp = phoenix.fs.resolvepathabsolute("./lsp")

                local data = {
                    ["luau-lsp.types.definitionFiles"] = {
                        phoenix = `{ lsp }/phoenix.d.luau`,
                    },
                    ["luau-lsp.types.documentationFiles"] = {
                        `{ lsp }/phoenix.json`,
                    },
                    ["luau-lsp.sourcemap.enabled"] = false,
                }

                fs.createDirectory(folderPath)
                local success, error = pcall(fs.writeStringToFile, `{ folderPath }/settings.json`, json.encode(data, 4))

                if not success then
                    ErrorMessage.Push(`Failed to create VS Code/Codium settings file: { error }`)
                end
            end
        end

        local result = process.system(`{ Preferences.CodeEditorPath } "{ phoenix.fs.resolvepathabsolute("@project") }"`).stdout

        if #result > 1 then
            warn("Open code editor result: ", result)
        end
    end
end

return ExternalCodeEditor
