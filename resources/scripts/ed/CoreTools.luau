local InputService: PlayerInput = game:GetService("PlayerInput") :: any
local HistoryService: History = game:GetService("History") :: any
local Engine: Engine = game:GetService("Engine") :: any

local CoreTools = {}
CoreTools.EditorCanUseMouse = true

local Types = require("./Types.luau")

local GizmoActive = true
local GizmoMode = 1
local GizmoMoveIncrement = 0.5

local GizmoContainer = GameObject.new("Model")
local GizmoPrevTarget: (GameObject & Transform)? = nil
local GizmoPrevTargetTransform = Matrix.identity
local GizmoPrevTargetSize = vector.zero
local GizmoPieceToDirection = {}
local GizmoPieces = {
    [vector.create( 1, 0, 0)] = { GameObject.new("Mesh"), GameObject.new("Mesh") },
    [vector.create(-1, 0, 0)] = { GameObject.new("Mesh"), GameObject.new("Mesh") },
    [vector.create(0,  1, 0)] = { GameObject.new("Mesh"), GameObject.new("Mesh") },
    [vector.create(0, -1, 0)] = { GameObject.new("Mesh"), GameObject.new("Mesh") },
    [vector.create(0, 0,  1)] = { GameObject.new("Mesh"), GameObject.new("Mesh") },
    [vector.create(0, 0, -1)] = { GameObject.new("Mesh"), GameObject.new("Mesh") }
}

for direction, piece in GizmoPieces do
    local col = Color.new(math.abs(direction.x), math.abs(direction.y), math.abs(direction.z))
    piece[1].Tint, piece[2].Tint = col, col
    piece[1].Material, piece[2].Material = "unlit", "unlit"
    piece[1].Size, piece[2].Size = vector.one * 0.5, vector.one * 0.5
    piece[1].Parent, piece[2].Parent = GizmoContainer, GizmoContainer
    
    piece[2].Transparency = 0.5

    GizmoPieceToDirection[piece[1]] = direction
    GizmoPieceToDirection[piece[2]] = direction

    piece[1]:AddComponent("RigidBody")
    piece[2]:AddComponent("RigidBody")
end

GizmoContainer.Name = "GizmoContainer"

function CoreTools.RenderControls(TabButtonControl: (string, string, boolean?, boolean?, string?, string?) -> boolean)
    if TabButtonControl("Gizmo", "@etextures/editor-icons/Transform.png", true, GizmoActive) then
		GizmoActive = not GizmoActive
	end

	imgui.sameline()
    imgui.setnextitemwidth(80)
    local s = imgui.cursorposition()

	GizmoMode = imgui.combo("Gizmo", { "Move", "Scale" }, GizmoMode)

    imgui.setcursorposition(s.x, s.y + 24)
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
local BeganSurfaceDragAction: number? = nil
local BeganGizmoDragAction: number? = nil

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

function CoreTools.Update(Project: Types.Project)
    if not Project.Workspace then
        return
    end

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
                if not BeganSurfaceDragAction then
                    BeganSurfaceDragAction = HistoryService:TryBeginAction("SurfaceDrag")
                end

                target.Transform = Matrix.fromTranslation(worldHit.Position + worldHit.Normal * target.Size / 2)
            end

            IsSurfaceDragging = true
        end
    else
        IsSurfaceDragging = false

        if BeganSurfaceDragAction then
            HistoryService:FinishAction(BeganSurfaceDragAction)
            BeganSurfaceDragAction = nil
        end
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
            local trans = target.Transform * Matrix.fromTranslation(direction * target.Size * 0.5 + direction * vector.one)
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

        if BeganGizmoDragAction then
            HistoryService:FinishAction(BeganGizmoDragAction)
            BeganGizmoDragAction = nil
        end
    end

    if IsGizmoDragging then
        assert(GizmoDragHandle)

        local dist = vector.magnitude(GizmoDragHandle.Transform.Position - camPos)

        local oldMouseDir = workspace:ScreenPointToVector(PrevDragMousePos, dist)
        local diff = (vector.normalize(mouseVector) * dist) - oldMouseDir

        local move = vector.create(roundTo(diff.x, GizmoMoveIncrement), roundTo(diff.y, GizmoMoveIncrement), roundTo(diff.z, GizmoMoveIncrement)) * GizmoDragAxis

        if vector.magnitude(move) > 0 and not BeganGizmoDragAction then
            BeganGizmoDragAction = HistoryService:TryBeginAction("GizmoDrag")
        end

        target.Transform += move
        PrevDragMousePos = cpos
    end
end

return CoreTools
