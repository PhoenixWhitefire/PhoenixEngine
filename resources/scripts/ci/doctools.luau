-- doctools.luau, 22/09/2025
local docComments = json.parse(assert(fs.read("./gh-assets/wiki/doc-comments.json")));
local apiDump = json.parse(assert(fs.read("./apidump.json")));

assert(not engine.isheadless());

local function libTools()
    for lib, members in apiDump.ScriptEnv.Libraries do
        assert(typeof(lib) == "string")

        local docLib = docComments.ScriptEnv.Libraries[lib]
        assert(docLib)

        if imgui.treenode(lib) then
            docLib.Description = imgui.inputstring("Description", docLib.Description or "")

            for name, type in members do
                assert(typeof(name) == "string")

                docLib.Members[name] = docLib.Members[name] or {}
                local docMember = docLib.Members[name] :: { [string]: any }

                if typeof(docMember) == "string" then
                    docMember = { Description = docMember }
                    docLib.Members[name] = docMember
                end

                if typeof(docMember.In) == "string" then
                    docMember.In = string.split(docMember.In, ", ")
                end

                if imgui.treenode(name) then
                    imgui.text("Parameters")

                    imgui.pushid("params")

                    for index, param in (docMember.In or {}) do
                        imgui.pushid(name .. index)

                        if typeof(param) == "string" then
                            docMember.In[index] = imgui.inputstring(tostring(index), param)

                            if docMember.In[index] == "\\del" then
                                table.remove(docMember.In, index)
                                imgui.popid()
                                break
                            end

                            if imgui.button("<Default value>") then
                                docMember.In[index] = { docMember.In[index], "" }
                            end
                        else
                            param[1] = param[1] or ""
                            param[2] = param[2] or ""

                            docMember.In[index][1] = imgui.inputstring(tostring(index) .. " type", param[1])
                            docMember.In[index][2] = imgui.inputstring(tostring(index) .. " default", param[2])

                            if docMember.In[index][1] == "\\del" then
                                table.remove(docMember.In, index)
                                imgui.popid()
                                break
                            end

                            if docMember.In[index][2] == "\\del" then
                                docMember.In[index] = param[1]
                            end
                        end

                        imgui.popid()
                    end

                    imgui.popid()

                    if imgui.button("+ Param") then
                        docMember.In = docMember.In or {}
                        table.insert(docMember.In, "")
                    end

                    local returns = imgui.inputstring("Outputs", docMember.Out or "")
                    docMember.Out = if #returns > 0 then returns else nil

                    if typeof(docMember.Description) == "string" then
                        docMember.Description = { docMember.Description }
                    end

                    imgui.text("Description");
                    imgui.pushid("desc")

                    for i, d in (docMember.Description or {}) do
                        docMember.Description[i] = imgui.inputstring(tostring(i), d)

                        if docMember.Description[i] == "\\del" then
                            table.remove(docMember.Description, i)
                            break
                        end
                    end

                    if imgui.button("+") then
                        docMember.Description = docMember.Description or {}
                        table.insert(docMember.Description, "")
                    end

                    imgui.popid()

                    imgui.treepop()
                end
            end

            imgui.treepop()
        end
    end
end

game.OnFrameBegin:Connect(function()
    if imgui.begin("Documentation Tools") then
        if imgui.button("Save") then
            fs.write("./gh-assets/wiki/doc-comments.json", json.encode(docComments))

            conf.set("LspDefinitionsGeneratorDoNotShutdown", true);
            local co, err = loadfile("scripts/ci/docs.luau", "LspDefinitionsGenerator")
            assert(co, err)

            coroutine.resume(co)
        end
    end
    imgui.endw()

    if imgui.begin("Documentation Tools: Libraries") then
        libTools()
    end
    imgui.endw()
end)
