local InputService: InputService = game:GetService("InputService") :: any
local Engine: Engine = game:GetService("Engine") :: any

local CoreTools = {}
CoreTools.EditorCanUseMouse = true

local GizmoActive = true
local GizmoMode = 1
local GizmoMoveIncrement = 0.5

local GizmoContainer = GameObject.new("Model" :: "Model")
local GizmoPrevTarget: (GameObject & Transform)? = nil
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
    
    piece[2].Transparency = 0.5

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

local PrevDragMousePos: vector? = nil
local IsSurfaceDragging = false
local IsGizmoDragging = false
local GizmoDragAxis = vector.zero
local GizmoDragHandle = nil

local function roundTo(n: number, x: number): number
    return math.round(n / x) * x
end

local function resetGizmo()
    GizmoContainer.Parent = nil
    GizmoPrevTarget = nil
    GizmoPrevTargetTransform = Matrix.identity
    GizmoPrevTargetSize = vector.zero
    PrevDragMousePos = nil
end

function CoreTools.Update(Project: { Workspace: Workspace })
    local selections = Engine:GetExplorerSelections()
    local target: (GameObject & Transform)? = (if selections[1] and selections[1]:HasComponent("Transform") then selections[1] else nil) :: any

    if IsSurfaceDragging or IsGizmoDragging then
        if GizmoPrevTarget and (not target or target ~= GizmoPrevTarget) then
            Engine:SetExplorerSelections({ GizmoPrevTarget })
            target = GizmoPrevTarget
        end
    end

    if target and not target.Exists then
        target = nil
    end

    if not target or not workspace.SceneCamera then
        resetGizmo()
        return
    end

    local camPos = workspace.SceneCamera.Transform.Position
    local cpos = InputService:GetCursorPosition()

    local mouseVector = workspace:ScreenPointToVector(cpos, 1000)
    local gizmoHit = if InputService:IsMouseButtonPressed(Enum.MouseButton.Left) then workspace:Raycast(camPos, mouseVector, { GizmoContainer }, false) else nil

    if InputService:IsMouseButtonPressed(Enum.MouseButton.Left) and not IsGizmoDragging and not gizmoHit then
        local worldHit = Project.Workspace:Raycast(camPos, mouseVector, { if IsSurfaceDragging then target else nil })

        if worldHit and (IsSurfaceDragging or worldHit.Object == target) then
            assert(worldHit)

            if not IsSurfaceDragging then
                PrevDragMousePos = cpos
            end

            if PrevDragMousePos ~= cpos then
               target.Transform = Matrix.fromTranslation(worldHit.Position + worldHit.Normal * target.Size / 2)
            end

            IsSurfaceDragging = true
        end
    else
        IsSurfaceDragging = false
    end

    if IsSurfaceDragging then
        GizmoContainer.Parent = nil
    else
        GizmoContainer.Parent = workspace
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

    GizmoPrevTarget = target
    GizmoPrevTargetTransform = target.Transform
    GizmoPrevTargetSize = target.Size

    if gizmoHit and InputService:IsMouseButtonPressed(Enum.MouseButton.Left) then
        if not IsGizmoDragging then
            PrevDragMousePos = cpos
            GizmoDragAxis = vector.abs(assert(GizmoPieceToDirection[gizmoHit.Object]))
            GizmoDragHandle = gizmoHit.Object

            for p, _ in GizmoPieceToDirection do
                p.Transparency = 0.8
            end

            GizmoDragHandle.Transparency = 0
        end

        IsGizmoDragging = true

    elseif not InputService:IsMouseButtonPressed(Enum.MouseButton.Left) then
        IsGizmoDragging = false

        for _, pcs in GizmoPieces do
            pcs[1].Transparency = 0
            pcs[2].Transparency = 0.5
        end
    end

    if IsGizmoDragging then
        local dist = vector.magnitude(GizmoDragHandle.Transform.Position - camPos)

        local oldMouseDir = workspace:ScreenPointToVector(PrevDragMousePos, dist)
        local diff = (vector.normalize(mouseVector) * dist) - oldMouseDir

        target.Transform += vector.create(roundTo(diff.x, GizmoMoveIncrement), roundTo(diff.y, GizmoMoveIncrement), roundTo(diff.z, GizmoMoveIncrement)) * GizmoDragAxis

        PrevDragMousePos = cpos
    end
end

return CoreTools
