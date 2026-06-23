local DeveloperTools = game:GetService("DeveloperTools")
local AssetManager = game:GetService("AssetManager")
local PlayerInput = game:GetService("PlayerInput")
local History = game:GetService("History")

local CoreTools = {}
CoreTools.HoveringOverCamera = nil :: (GameObject & EcTransform)?
CoreTools.VisualizationsContainer = GameObject.new("Model")
CoreTools.VisualizationsContainer.Parent = workspace
CoreTools.VisualizationTargets = {} :: { [GameObject & EcTransform]: GameObject }

local cam = assert(AssetManager:LoadScene("./resources/scenes/camera.hxscene"))[1] :: (GameObject & EcTransform)?
CoreTools.CameraModel = assert(cam)

local Types = require("./Types")

local GizmoActive = true
local GizmoMode = 1
local GizmoMoveIncrement = 0.5

local GizmoContainer = GameObject.new("Model")
local GizmoPrevTarget: (GameObject & EcTransform)? = nil
local GizmoPrevTargetTransform = Matrix.identity
local GizmoPrevTargetSize = vector.zero
local GizmoPieceToDirection: { [GameObject & EcMesh]: vector } = {}
local GizmoPieces = {
    [vector.create( 1,  0,  0)] = { GameObject.new("Mesh"), GameObject.new("Mesh") },
    [vector.create(-1,  0,  0)] = { GameObject.new("Mesh"), GameObject.new("Mesh") },
    [vector.create( 0,  1,  0)] = { GameObject.new("Mesh"), GameObject.new("Mesh") },
    [vector.create( 0, -1,  0)] = { GameObject.new("Mesh"), GameObject.new("Mesh") },
    [vector.create( 0,  0,  1)] = { GameObject.new("Mesh"), GameObject.new("Mesh") },
    [vector.create( 0,  0, -1)] = { GameObject.new("Mesh"), GameObject.new("Mesh") }
}

for direction, piece in GizmoPieces do
    local col = Color.new(math.abs(direction.x), math.abs(direction.y), math.abs(direction.z))
    piece[1].Tint, piece[2].Tint = col, col
    piece[1].Material, piece[2].Material = "unlit", "unlit"
    piece[1].Size, piece[2].Size = vector.one * 0.25, vector.one * 0.25
    piece[1].Name, piece[2].Name = `Gizmo_{direction}_1`, `Gizmo_{direction}_2`
    piece[1].MeshAsset, piece[2].MeshAsset = "!Cone", "!Cone"
    piece[1].Parent, piece[2].Parent = GizmoContainer, GizmoContainer

    piece[2].Transparency = 0.5

    GizmoPieceToDirection[piece[1]] = direction
    GizmoPieceToDirection[piece[2]] = direction
end

GizmoContainer.Name = "GizmoContainer"

local DefaultCameraModelColors: { [string]: Color } = {}

for _, g in CoreTools.CameraModel:GetChildren() do
    if g:HasComponent("Mesh") then
        local m = g :: EcMesh
        DefaultCameraModelColors[g.Name] = m.Tint
    end
end

function CoreTools.RenderControls(TabButtonControl: (string, string, boolean?, boolean?, string?, string?) -> boolean)
    if TabButtonControl("Gizmo", "@etextures/editor-icons/Transform.png", true, GizmoActive) then
		GizmoActive = not GizmoActive
	end

	imgui.sameline()
    imgui.setnextitemwidth(80)
    local s = imgui.cursorposition()

	local gizmoMode, changed = imgui.combo("Gizmo", { "Move", "Scale" }, GizmoMode)

    if changed then
        GizmoMode = gizmoMode

        local shape = if gizmoMode == 1 then "!Cone" else "!Sphere"

        for _, ps in GizmoPieces do
            ps[1].MeshAsset, ps[2].MeshAsset = shape, shape
        end
    end

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
local GizmoDragHandle: (GameObject & EcMesh & EcTransform)? = nil
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

local function keepGizmoInView(target: GameObject & EcTransform, ViewportRenderPosition: vector, ViewportRenderSize: vector, DeltaTime: number)
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

    cam.Transform *= Matrix.rotated(-dy * orbitSpeed * DeltaTime, -dx * orbitSpeed * DeltaTime, 0)
    cam.Transform *= Matrix.rotated(0, 0, -cam.Transform:ExtractAngles().z)

    if dx ~= 0 or dy ~= 0 then
        GizmoDragStartObjectPosition = target.Transform.Position
    end

    local axisLocal = assert(GizmoPieceToDirection[GizmoDragHandle])
    local rotOnly = target.Transform - target.Transform.Position
    local axisWorld = vector.normalize(rotOnly * axisLocal)
    GizmoDragPlaneNormal = vector.normalize(vector.cross(axisWorld, cam.Transform.Forward))
end

function CoreTools.Update(Project: Types.Project, EditorPreferences, ViewportRenderPosition: vector, ViewportRenderSize: vector, DeltaTime: number)
    if not Project.Workspace then
        return
    end

    local selections = DeveloperTools:GetExplorerSelections()
    local target: (GameObject & EcTransform)? = (if selections[1] and selections[1]:HasComponent("Transform") then selections[1] else nil) :: any

    if IsSurfaceDragging or IsGizmoDragging then
        if GizmoPrevTarget and (not target or target ~= GizmoPrevTarget) then
            DeveloperTools:SetExplorerSelections({ GizmoPrevTarget })
            target = GizmoPrevTarget
        end
    end

    local tryingToInteract = not PlayerInput.IsMouseSunk and PlayerInput:IsMouseButtonPressed(Enum.MouseButton.Left)

    local cam = workspace.SceneCamera
    local camPos = cam.Transform.Position
    local cpos = PlayerInput:GetCursorPosition()

    local mouseVector = workspace:ScreenPointToVector(cpos, 1000)

    local cameraHit = if not IsSurfaceDragging and not IsGizmoDragging
                        then workspace:Raycast(camPos, mouseVector, { CoreTools.VisualizationsContainer }, false)
                        else nil
    local camObj = if cameraHit then assert(cameraHit.Object.Parent) :: GameObject & EcTransform else nil

    if camObj then
        if camObj.Enabled then
            assert(camObj:HasComponent("Transform"))
        else
            camObj = nil
        end
    end

    if cameraHit and CoreTools.HoveringOverCamera ~= camObj and camObj then
        local hidden = vector.magnitude(workspace.SceneCamera.Transform.Position - camObj.Transform.Position) < 0.5

        for _, g in camObj:GetChildren() do
            if g:HasComponent("Mesh") then
                local m = g :: EcMesh
                m.Material = "@base/materials/unlit.mtl"
                m.Tint = Color.new(1, 1, 1)

                if hidden then
                    m.Transparency = 1
                elseif g.Name:find("I") then
                    m.Transparency = 0.5
                end
            end
        end

        if CoreTools.HoveringOverCamera then
            for _, g in CoreTools.HoveringOverCamera:GetChildren() do
                if g:HasComponent("Mesh") then
                    local m = g :: EcMesh
                    m.Material = "@base/materials/plastic.mtl"
                    m.Tint = DefaultCameraModelColors[g.Name]
                end
            end
        end
    elseif not cameraHit and CoreTools.HoveringOverCamera then
        for _, g in CoreTools.HoveringOverCamera:GetChildren() do
            if g:HasComponent("Mesh") then
                local m = g :: EcMesh
                m.Material = "@base/materials/plastic.mtl"
                m.Tint = DefaultCameraModelColors[g.Name]
            end
        end
    end

    if camObj and tryingToInteract then
        DeveloperTools:SetExplorerSelections({ assert(CoreTools.VisualizationTargets[camObj]) })
    end

    CoreTools.HoveringOverCamera = camObj

    if not target or not workspace.SceneCamera or target == workspace.SceneCamera then
        resetGizmo()
        return
    end

    if PlayerInput:IsKeyPressed(Enum.Key.F) and not PlayerInput.IsKeyboardSunk then
        local pos = target.Transform.Position - (cam.Transform.Forward * math.max(target.Size.x, target.Size.y, target.Size.z) * 1.5)
        -- TODO fix 180* flip
        cam.Transform = Matrix.lookAt(pos, target.Transform.Position) * Matrix.rotated(0, math.pi, 0)
    end

    local gizmoHit = if tryingToInteract
                        then workspace:Raycast(camPos, mouseVector, { GizmoContainer }, false)
                        else nil

    if tryingToInteract and not IsGizmoDragging and not gizmoHit then
        local worldHit = Project.Workspace:Raycast(camPos, mouseVector, if IsSurfaceDragging then { target } else {})

        if worldHit and (IsSurfaceDragging or worldHit.Object == target) then
            assert(worldHit)

            if not IsSurfaceDragging then
                PrevDragMousePos = cpos
            end

            if PrevDragMousePos ~= cpos then
                if not BeganSurfaceDragAction then
                    BeganSurfaceDragAction = History:TryBeginAction("SurfaceDrag")
                end

                target.Transform = Matrix.translated(worldHit.Position + worldHit.Normal * target.Size / 2)
            end

            IsSurfaceDragging = true
        end
    else
        IsSurfaceDragging = false

        if BeganSurfaceDragAction then
            History:FinishAction(BeganSurfaceDragAction)
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
        GizmoContainer.Transform = target.Transform

        for direction, piece in GizmoPieces do
            local trans = Matrix.lookAt(direction * 0.75, vector.create(0.0001, 0, 0)) * Matrix.rotated(math.rad(90), 0, 0)
            piece[1].LocalTransform, piece[2].LocalTransform = trans, trans
        end
    end

    GizmoContainer.Size = vector.one * vector.magnitude(workspace.SceneCamera.Transform.Position - GizmoContainer.Transform.Position) / workspace.SceneCamera.FieldOfView * 60

    GizmoPrevTarget = target
    GizmoPrevTargetTransform = target.Transform
    GizmoPrevTargetSize = target.Size

    if tryingToInteract then
        if not IsGizmoDragging and gizmoHit then
            assert(gizmoHit.Object:HasComponent("Mesh"))
            local hit: GameObject & EcMesh & EcTransform = gizmoHit.Object :: any

            PrevDragMousePos = cpos

            local axisLocal = assert(GizmoPieceToDirection[hit])
            -- TODO luau manual annotation is luau bug (inferring `never`)
            local rotOnly: vector = target.Transform - target.Transform.Position
            local axisWorld = vector.normalize(rotOnly * axisLocal)

            GizmoDragAxis = assert(GizmoPieceToDirection[hit])
            GizmoDragHandle = hit

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

    else
        IsGizmoDragging = false

        for _, pcs in GizmoPieces do
            pcs[1].Transparency = 0
            pcs[2].Transparency = 0.5
        end

        if BeganGizmoDragAction then
            History:FinishAction(BeganGizmoDragAction)
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
            BeganGizmoDragAction = History:TryBeginAction("GizmoDrag")
        end

        if GizmoMode == 1 then
            target.Transform = GizmoDragStartTransform * Matrix.translated(move)
        else
            local newSize = GizmoDragStartSize + GizmoPieceToDirection[GizmoDragHandle] * amount * math.sign(delta.x + delta.y + delta.z)

            if newSize.x > 0 and newSize.y > 0 and newSize.z > 0 then
                target.Size = newSize
                target.Transform = GizmoDragStartTransform * Matrix.translated(move * 0.5)
            end
        end

        PrevDragMousePos = cpos

        if EditorPreferences.KeepGizmoInView then
            keepGizmoInView(target, ViewportRenderPosition, ViewportRenderSize, DeltaTime)
        end
    end
end

return CoreTools
