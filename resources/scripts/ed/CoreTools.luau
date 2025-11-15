local CoreTools = {}
CoreTools.EditorCanUseMouse = true

local GizmoActive = true
local GizmoMode = 1
local GizmoMoveIncrement = 0.5

local GizmoContainer = GameObject.new("Model" :: "Model")
local GizmoPrevTarget = nil
local GizmoPrevTargetTransform = Matrix.identity
local GizmoPrevTargetSize = vector.zero
local GizmoPieceToDirection = {}
local GizmoPieces = {
    [vector.create( 1, 0, 0)] = { GameObject.new("Mesh" :: "Mesh"), GameObject.new("Mesh" :: "Mesh") },
    [vector.create(-1, 0, 0)] = { GameObject.new("Mesh" :: "Mesh"), GameObject.new("Mesh" :: "Mesh") },
    [vector.create(0,  1, 0)] = { GameObject.new("Mesh" :: "Mesh"), GameObject.new("Mesh" :: "Mesh") },
    [vector.create(0, -1, 0)] = { GameObject.new("Mesh" :: "Mesh"), GameObject.new("Mesh" :: "Mesh") },
    [vector.create(0, 0,  1)] = { GameObject.new("Mesh" :: "Mesh"), GameObject.new("Mesh" :: "Mesh") },
    [vector.create(0, 0, -1)] = { GameObject.new("Mesh" :: "Mesh"), GameObject.new("Mesh" :: "Mesh") }
}

for direction, piece in GizmoPieces do
    local col = Color.new(math.abs(direction.x), math.abs(direction.y), math.abs(direction.z))
    piece[1].Tint, piece[2].Tint = col, col
    piece[1].Material, piece[2].Material = "unlit", "unlit"
    piece[1].Parent, piece[2].Parent = GizmoContainer, GizmoContainer
    
    piece[2].Transparency = .8
    piece[1].Transparency = 0

    GizmoPieceToDirection[piece[1]] = direction
    GizmoPieceToDirection[piece[2]] = direction
end

GizmoContainer.Name = "GizmoContainer"

function CoreTools.RenderControls(TabButtonControl: (string, string, boolean?, boolean?, string?) -> boolean)
    if TabButtonControl("Gizmo", "@etextures/editor-icons/Transform.png", true, GizmoActive) then
		GizmoActive = not GizmoActive
	end

	imgui.sameline()
    imgui.setnextitemwidth(80)
    local sx, sy = imgui.cursorposition()

	GizmoMode = imgui.combo("Gizmo", { "Move", "Scale" }, GizmoMode)

    imgui.setcursorposition(sx, sy + 24)
    imgui.setnextitemwidth(80)
    GizmoMoveIncrement = imgui.inputnumber("Move Increment", GizmoMoveIncrement)
end

local PrevGizmoActive = GizmoActive

function CoreTools.OnEditorStageChanged(NewStage)
    if NewStage == "ProjectPlaying" then
        PrevGizmoActive = GizmoActive
        GizmoActive = false

    elseif NewStage == "ProjectEditing" then
        GizmoActive = PrevGizmoActive
    end
end

local PrevDragMouseX, PrevDragMouseY = nil, nil
local IsDragging = false

local function resetGizmo()
    GizmoContainer.Parent = nil
    GizmoPrevTarget = nil
    GizmoPrevTargetTransform = Matrix.identity
    GizmoPrevTargetSize = vector.zero
    PrevDragMouseX, PrevDragMouseY = nil, nil
end

function CoreTools.Update(Project)
    local selections = engine.explorerselections()
    local target: (GameObject & Transform)? = (if selections[1] and selections[1]:HasComponent("Transform") then selections[1] else nil) :: any

    if not target then
        resetGizmo()
        return
    end

    local camPos = workspace.SceneCamera.Transform.Position
    local mx, my = input.mouseposition()

    local mouseVector = workspace:ScreenPointToRay(mx, my, 1000)
    local edHit = workspace:Raycast(camPos, mouseVector)

    if not edHit and not input.guihandledm() and input.mousedown('l') then
        local worldHit = Project.Workspace:Raycast(camPos, mouseVector)

        if worldHit.Object == target then
            if PrevDragMouseX and PrevDragMouseY then
                if PrevDragMouseX ~= mx or PrevDragMouseY ~= my then
                    target.Transform = Matrix.fromTranslation(worldHit.Position + worldHit.Normal * target.Size)
                end
            else
                PrevDragMouseX, PrevDragMouseY = mx, my
            end
        else
            PrevDragMouseX, PrevDragMouseY = nil, nil
        end
    else
        PrevDragMouseX, PrevDragMouseY = nil, nil
    end

    if not GizmoActive then
        resetGizmo()
        return
    end

    if target ~= GizmoPrevTarget or target.Transform ~= GizmoPrevTargetTransform or target.Size ~= GizmoPrevTargetSize then
        for direction, piece in GizmoPieces do
            local trans = target.Transform * Matrix.fromTranslation(direction * target.Size * 0.5 + direction * vector.create(2, 2, 2))
            piece[1].Transform, piece[2].Transform = trans, trans
        end
    end

    GizmoContainer.Parent = workspace

    GizmoPrevTarget = target
    GizmoPrevTargetTransform = target.Transform
    GizmoPrevTargetSize = target.Size
end

return CoreTools
