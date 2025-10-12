-- doctools.luau, 22/09/2025
local docComments = json.parse(assert(fs.read("./gh-assets/wiki/doc-comments.json")));
local apiDump = json.parse(assert(fs.read("./apidump.json")));

assert(not engine.isheadless());

local function keysOf(t)
    local keys = {}

    for k, _ in t do
        table.insert(keys, k)
    end

    return keys
end

local VALID_TYPES = {
    any = true,
    number = true,
    string = true,
    boolean = true,
    vector = true,
    GameObject = true,
    Matrix = true,
    Color = true,

    Model = true,
}

local function isTypeValid(type: string) : (boolean, string?)
    local lexeme = ""
    local inDoubleQuote = false
    local inSingleQuote = false
    local parenthesesDepth = 0
    local tableDepth = 0
    local combinable = true
    local firstLex = true
    local prevLexemes = {}
    local prevCombiners = { "_START" }

    type ..= " "

    for i = 1, #type do
        local c = string.sub(type, i, i)

        if c == "(" then
            parenthesesDepth += 1
        end

        if c == ")" then
            parenthesesDepth -= 1
        end

        if c == "{" then
            tableDepth += 1
        end

        if c == "}" then
            tableDepth -= 1
        end

        if parenthesesDepth < 0 then
            return false, `Too many closing parentheses (last lexeme: { lexeme })`
        end

        if tableDepth < 0 then
            return false, `Too many closing curly brackets (last lexeme: { lexeme })`
        end

        if c == " " or c == "," or c == "(" or c == ")" or c == "{" or c == "}" or c == "?" or c == "|" or c == "&" or c == ":" then
            if c ~= ":" and c ~= "," and c ~= "&" and c ~= "|" and #lexeme > 0 and not VALID_TYPES[lexeme] then
                return false, `Invalid type '{ lexeme }'`

            elseif c == ":" then
                if combinable and #lexeme == 0 then
                    return false, `Combiner '{ c }' at char { i } directly follows previous combiner '{ prevCombiners[#prevCombiners] }'`
                end

                table.insert(prevLexemes, lexeme)
                table.insert(prevCombiners, ":")
                firstLex = false

                combinable = true
                lexeme = ""
                continue

            elseif (c == "|" or c == "&") and #lexeme > 0 then
                if combinable then
                    --assert(false)
                    --return false, `Combiner '{ c }' at char { i } directly follows previous combiner '{ prevCombiners[#prevCombiners] }'`
                end

                combinable = true
                table.insert(prevCombiners, "| or &")

            elseif c == "," and #lexeme > 0 then
                if combinable then
                    --return false, `Combiner '{ c }' at char { i } directly follows previous combiner '{ prevCombiners[#prevCombiners] }'`
                end

                combinable = true
                table.insert(prevCombiners, ",")

            elseif #lexeme > 0 and c ~= " " then
                if not combinable and not firstLex then
                    assert(false)
                    return false, `Cannot combine type '{ lexeme }' with the previous type because there is no combiner ('&', '|' or ',')`
                end

                table.insert(prevLexemes, lexeme)
                firstLex = false

                lexeme = ""
                combinable = false

                continue

            else
                lexeme = ""
            end

            continue
        end

        if c == '"' then
            table.insert(prevLexemes, lexeme)

            inDoubleQuote = not inDoubleQuote
            lexeme = ""

            if not inDoubleQuote then
                combinable = false
            end

            continue
        end

        if c == "'" then
            table.insert(prevLexemes, lexeme)

            inSingleQuote = not inSingleQuote
            lexeme = ""

            if not inSingleQuote then
                combinable = false
            end

            continue
        end

        lexeme ..= c
    end

    if inSingleQuote then
        return false, "Unclosed single quote"
    end

    if inDoubleQuote then
        return false, "Unclosed double quote"
    end

    if parenthesesDepth > 0 then
        return false, `Didn't close { parenthesesDepth } parentheses(s)`
    end

    if parenthesesDepth < 0 then
        return false, `{ -parenthesesDepth } too many parentheses`
    end

    if tableDepth > 0 then
        return false, `Didn't close { tableDepth } table(s)`
    end

    if tableDepth < 0 then
        return false, `{ -tableDepth } too many curly braces`
    end

    return true, nil
end

local function areMemberTypesValid(member)
    for _, param in (if typeof(member.In) == "string" then { member.In } else (member.In or {})) do
        if typeof(param) == "string" then
            if not isTypeValid(param) then
                return false
            end
        else
            assert(typeof(param) == "table")

            if not isTypeValid(param[1]) then
                return false
            end
        end
    end

    if not isTypeValid(member.Out or "") then
        return false
    end

    return true
end

local function isLibFullyDescribed(docLib: any)
    if not docLib.Description or #docLib.Description <= 2 then
        return false
    end

    for _, member in docLib.Members do
        if not member.Description then
            return false
        end

        if typeof(member.Description) == "table" then
            if #member.Description == 0 or #member.Description[1] <= 2 then
                return false
            end
        else
            assert(typeof(member.Description) == "string")

            if #member.Description <= 2 then
                return false
            end
        end

        if not areMemberTypesValid(member :: any) then
            return false
        end
    end

    return true
end

local function libTools()
    local libnames = keysOf(apiDump.ScriptEnv.Libraries)
    table.sort(libnames)

    for _, lib in libnames do
        assert(typeof(lib) == "string")

        local members = apiDump.ScriptEnv.Libraries[lib]
        local docLib = docComments.ScriptEnv.Libraries[lib]
        assert(docLib)

        local hadDescription = isLibFullyDescribed(docLib)

        if not hadDescription then
            imgui.pushstylecolor(0, { 1, 0, 0, 1 })
        end

        if imgui.treenode(lib) then
            if not hadDescription then
                imgui.popstylecolor()
            end

            local hadLibDescription = docLib.Description

            if not hadLibDescription then
                imgui.pushstylecolor(0, { 1, 0, 0, 1 })
            end

            local desc = imgui.inputstring("Description", docLib.Description or "")
            docLib.Description = if #desc > 0 then desc else nil

            if not hadLibDescription then
                imgui.popstylecolor()
            end

            local memberNames = keysOf(members)
            table.sort(memberNames)

            for _, name in memberNames do
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

                local memberError = not docMember.Description or not areMemberTypesValid(docMember)

                if memberError then
                    imgui.pushstylecolor(0, { 1, 0, 0, 1 })
                end

                if imgui.treenode(name) then
                    if memberError then
                        imgui.popstylecolor()
                    end

                    imgui.separatortext("Parameters")

                    for index, param in (docMember.In or {}) do
                        imgui.pushid(name .. index)

                        if typeof(param) == "string" then
                            local tvalid, err = isTypeValid(param)

                            if not tvalid then
                                imgui.pushstylecolor(0, { 1, 0, 0, 1 })
                            end

                            docMember.In[index] = imgui.inputstring(tostring(index), param)

                            if not tvalid then
                                imgui.setitemtooltip(assert(err))
                                imgui.popstylecolor()
                            end

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

                            local tvalid, err = isTypeValid(param[1])

                            if not tvalid then
                                imgui.pushstylecolor(0, { 1, 0, 0, 1 })
                            end

                            docMember.In[index][1] = imgui.inputstring(tostring(index) .. " type", param[1])

                            if not tvalid then
                                imgui.setitemtooltip(assert(err))
                                imgui.popstylecolor()
                            end

                            docMember.In[index][2] = imgui.inputstring(tostring(index) .. " default", param[2])
                            
                            local paramstr = docMember.In[index][1]
                            if string.sub(paramstr, #paramstr, #paramstr) ~= "?" then
                                imgui.text("(Are you sure you haven't forgotten to end the parameter with a '?'?)")
                            end

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

                    if imgui.button("+ Param") then
                        docMember.In = docMember.In or {}
                        table.insert(docMember.In :: { string }, "")
                    end

                    imgui.separatortext("Outputs")

                    local outputTypeValid, outputTypeError = isTypeValid(docMember.Out or "")

                    if not outputTypeValid then
                        imgui.pushstylecolor(0, { 1, 0, 0, 1 })
                    end

                    local returns = imgui.inputstring("Outputs", docMember.Out or "")
                    docMember.Out = if #returns > 0 then returns else nil

                    if not outputTypeValid then
                        imgui.setitemtooltip(assert(outputTypeError))
                        imgui.popstylecolor()
                    end

                    if typeof(docMember.Description) == "string" then
                        docMember.Description = { docMember.Description }
                    end

                    local hadMemberDescription = docMember.Description

                    if not hadMemberDescription then
                        imgui.pushstylecolor(0, { 1, 0, 0, 1 })
                    end

                    imgui.separatortext("Description");

                    for i, d in (docMember.Description or {}) do
                        docMember.Description[i] = imgui.inputstring(tostring(i), d)

                        if docMember.Description[i] == "\\del" then
                            table.remove(docMember.Description, i)
                            break
                        end
                    end

                    if not hadMemberDescription then
                        imgui.popstylecolor()
                    end

                    if imgui.button("+") then
                        docMember.Description = docMember.Description or {}
                        table.insert(docMember.Description, "")
                    end

                    imgui.treepop()
                else
                    if memberError then
                        imgui.popstylecolor()
                    end
                end
            end

            imgui.treepop()
        else
            if not hadDescription then
                imgui.popstylecolor()
            end
        end
    end
end

game.OnFrameBegin:Connect(function()
    if imgui.begin("Documentation Tools") then
        if imgui.button("Save") then
            fs.write("./gh-assets/wiki/doc-comments.json", json.encode(docComments))

            conf.set("LspDefinitionsGeneratorDoNotShutdown", true);
            local co, err = loadthreadfromfile("scripts/ci/docs.luau", "LspDefinitionsGenerator")
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
