local PlayerInput: PlayerInput = game:GetService("PlayerInput") :: any
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
local GizmoDragStartWorldPoint = nil
local GizmoDragStartObjectPosition = nil
local GizmoDragStartTransform = nil
local GizmoDragStartSize = vector.one
local GizmoDragPlaneNormal = nil
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
    GizmoDragStartWorldPoint = nil
    GizmoDragStartObjectPosition = nil
    GizmoDragPlaneNormal = nil
    GizmoDragStartTransform = nil
end

function rayPlaneIntersect(rayOrigin: vector, rayDir: vector, planePoint: vector, planeNormal: vector): vector?
    local denom = vector.dot(rayDir, planeNormal)
    if math.abs(denom) < 1e-6 then
        return nil
    end

    local t = vector.dot(planePoint - rayOrigin, planeNormal) / denom
    if t < 0 then
        return nil
    end

    return rayOrigin + rayDir * t
end

local function findDxDyToKeepOnScreen(Point: vector, ViewportRenderPosition: vector, ViewportRenderSize: vector): (number, number)
    local screenPos = workspace:WorldToScreenPoint(Point) - ViewportRenderPosition

    local margin = 120
    local width = ViewportRenderSize.x
    local height = ViewportRenderSize.y

    local dx = 0
    local dy = 0

    if screenPos.x < margin then
        dx = -(margin - screenPos.x) / margin
    elseif screenPos.x > width - margin then
        dx = (screenPos.x - (width - margin)) / margin
    end

    if screenPos.y < margin then
        dy = -(margin - screenPos.y) / margin
    elseif screenPos.y > height - margin then
        dy = (screenPos.y - (height - margin)) / margin
    end

    return dx, dy
end

local function keepGizmoInView(target: GameObject & Transform, ViewportRenderPosition: vector, ViewportRenderSize: vector, DeltaTime: number)
    assert(GizmoDragHandle)

    local cam = workspace.SceneCamera

    local orbitSpeed = math.rad(15) -- degrees/sec
    local dx, dy = 0, 0

    for _, v in GizmoPieceToDirection do
        local tdx, tdy = findDxDyToKeepOnScreen(v / 2 + target.Transform.Position, ViewportRenderPosition, ViewportRenderSize)

        if math.abs(tdx) > math.abs(dx) then
            dx = tdx
        end
        if math.abs(tdy) > math.abs(dy) then
            dy = tdy
        end
    end

    cam.Transform *= Matrix.fromAngles(-dy * orbitSpeed * DeltaTime, -dx * orbitSpeed * DeltaTime, 0)
    cam.Transform *= Matrix.fromAngles(0, 0, -cam.Transform:ExtractAngles().z)

    if dx ~= 0 or dy ~= 0 then
        GizmoDragStartObjectPosition = target.Transform.Position
    end

    local axisLocal = assert(GizmoPieceToDirection[GizmoDragHandle])
    local rotOnly = target.Transform - target.Transform.Position
    local axisWorld = vector.normalize((rotOnly * axisLocal).Position)
    GizmoDragPlaneNormal = vector.normalize(vector.cross(axisWorld, cam.Transform.Forward))
end

function CoreTools.Update(Project: Types.Project, EditorPreferences, ViewportRenderPosition: vector, ViewportRenderSize: vector, DeltaTime: number)
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

    if not target or not workspace.SceneCamera then
        resetGizmo()
        return
    end

    local cam = workspace.SceneCamera
    local camPos = cam.Transform.Position
    local cpos = PlayerInput:GetCursorPosition()

    if PlayerInput:IsKeyPressed(Enum.Key.F) then
        local pos = target.Transform.Position - (cam.Transform.Forward * math.max(target.Size.x, target.Size.y, target.Size.z) * 1.5)
        -- TODO fix 180* flip
        cam.Transform = Matrix.lookAt(pos, target.Transform.Position) * Matrix.fromAngles(0, math.pi, 0)
    end

    local tryingToInteract = not PlayerInput.IsMouseSunk and PlayerInput:IsMouseButtonPressed(Enum.MouseButton.Left)

    local mouseVector = workspace:ScreenPointToVector(cpos, 1000)
    local gizmoHit = if tryingToInteract
                        then workspace:Raycast(camPos, mouseVector, { GizmoContainer }, false)
                        else nil

    if tryingToInteract and not IsGizmoDragging and not gizmoHit then
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

    if tryingToInteract then
        if not IsGizmoDragging and gizmoHit then
            PrevDragMousePos = cpos

            local axisLocal = assert(GizmoPieceToDirection[gizmoHit.Object])
            local rotOnly = target.Transform - target.Transform.Position
            local axisWorld = vector.normalize((rotOnly * axisLocal).Position)

            GizmoDragAxis = assert(GizmoPieceToDirection[gizmoHit.Object])
            GizmoDragHandle = gizmoHit.Object

            GizmoDragStartTransform = target.Transform
            GizmoDragStartObjectPosition = target.Transform.Position
            GizmoDragStartSize = target.Size

            local camForward = cam.Transform.Forward
            GizmoDragPlaneNormal = vector.normalize(vector.cross(axisWorld, camForward))

            GizmoDragStartWorldPoint = rayPlaneIntersect(camPos, mouseVector, target.Transform.Position, GizmoDragPlaneNormal)

            for p, _ in GizmoPieceToDirection do
                p.Transparency = 0.8
            end

            GizmoDragHandle.Transparency = 0
            IsGizmoDragging = true
        end

    elseif not tryingToInteract then
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

    local hit = IsGizmoDragging and GizmoDragStartObjectPosition and GizmoDragPlaneNormal and GizmoDragStartWorldPoint and rayPlaneIntersect(camPos, mouseVector, GizmoDragStartObjectPosition, GizmoDragPlaneNormal)

    if hit then
        assert(GizmoDragHandle)
        assert(GizmoDragStartWorldPoint)
        assert(GizmoDragStartObjectPosition)
        assert(GizmoDragStartTransform)

        local delta = hit - GizmoDragStartWorldPoint
        local projected = vector.dot(delta, GizmoDragAxis)
        local amount = roundTo(projected, GizmoMoveIncrement)
        local move = GizmoDragAxis * amount

        if vector.magnitude(move) > 0 and not BeganGizmoDragAction then
            BeganGizmoDragAction = HistoryService:TryBeginAction("GizmoDrag")
        end

        if GizmoMode == 1 then
            target.Transform = GizmoDragStartTransform * Matrix.fromTranslation(move)
        else
            local newSize = GizmoDragStartSize + GizmoPieceToDirection[GizmoDragHandle] * amount * math.sign(delta.x + delta.y + delta.z)

            if newSize.x > 0 and newSize.y > 0 and newSize.z > 0 then
                target.Size = newSize
                target.Transform = GizmoDragStartTransform * Matrix.fromTranslation(move * 0.5)
            end
        end

        PrevDragMousePos = cpos

        if EditorPreferences.KeepGizmoInView then
            keepGizmoInView(target, ViewportRenderPosition, ViewportRenderSize, DeltaTime)
        end
    end
end

return CoreTools
