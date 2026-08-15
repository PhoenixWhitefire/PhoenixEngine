local DeveloperTools = game:GetService("DeveloperTools")
local AssetManager = game:GetService("AssetManager")
local PlayerInput = game:GetService("PlayerInput")
local Workspace = game:GetService("Workspace")
local History = game:GetService("History")

local CoreTools = {}
CoreTools.HoveringOverCamera = nil :: (GameObject & EcTransform)?
CoreTools.VisualizationsContainer = GameObject.fromTemplate("Model")
CoreTools.VisualizationsContainer.Parent = Workspace
CoreTools.VisualizationTargets = {} :: { [GameObject & EcTransform]: GameObject }

local cam = assert(AssetManager:LoadScene("./resources/scenes/camera.hxscene"))[1] :: (GameObject & EcTransform)?
CoreTools.CameraModel = assert(cam)

local Types = require("./Types")

local GizmoActive = true
local GizmoMode = 1
local GizmoMoveIncrement = 0.5

local GizmoContainer = GameObject.fromTemplate("Model")
local GizmoPrevTarget: (GameObject & EcTransform)? = nil
local GizmoPrevTargetTransform = Matrix.identity
local GizmoPieceToDirection: { [GameObject]: vector } = {}
local ShadowMeshes = {}

local MoveGizmo = assert(AssetManager:LoadScene("./resources/scenes/movegizmo.hxscene"))[1] :: (GameObject & EcTransform & EcModel)?
assert(MoveGizmo)

local ScaleGizmo = assert(AssetManager:LoadScene("./resources/scenes/scalegizmo.hxscene"))[1] :: (GameObject & EcTransform & EcModel)?
assert(ScaleGizmo)

local Axes = {
    X = vector.create(1, 0, 0),
    Y = vector.create(0, 1, 0),
    Z = vector.create(0, 0, 1)
}

for _, gizmo in { MoveGizmo, ScaleGizmo } do
    for _, axis in { "X", "Y", "Z" } do
        local group = gizmo:FindChild(axis)
        assert(group)

        local clickable = group:FindChild("Clickable")
        assert(clickable and clickable:HasComponent("Transform") and clickable:HasComponent("RigidBody"))

        GizmoPieceToDirection[clickable] = Axes[axis]
        GizmoPieceToDirection[group] = Axes[axis]
    end

    for _, d in gizmo:GetDescendants() do
        local mesh = d:AsComponent("Mesh")
        if mesh then
            local shadow = mesh:Duplicate()
            shadow.Name ..= "_Shadow"
            ShadowMeshes[shadow] = mesh

            -- TODO luau
            assert(shadow:AsComponent("Mesh")).Transparency = 0.5
            shadow.Parent = mesh.Parent
        end
    end

    gizmo.Parent = GizmoContainer
end

MoveGizmo.Enabled = true
ScaleGizmo.Enabled = false
GizmoContainer.Name = "GizmoContainer"

local DefaultCameraModelColors: { [string]: Color } = {}

for _, g in CoreTools.CameraModel:GetChildren() do
    if g:HasComponent("Mesh") then
        local m = g :: EcMesh
        DefaultCameraModelColors[g.Name] = m.Tint
    end
end

function CoreTools.SetGizmoMode(Mode: number)
    GizmoMode = Mode
    MoveGizmo.Enabled = Mode == 1
    ScaleGizmo.Enabled = Mode == 2
end

function CoreTools.RenderControls(TabButtonControl: (string, string, boolean?, boolean?, string?, string?) -> boolean)
    if TabButtonControl("Gizmo", "@_etextures/editor-icons/Transform.png", true, GizmoActive) then
		GizmoActive = not GizmoActive
	end

	imgui.sameline()
    imgui.setnextitemwidth(80)
    local s = imgui.cursorposition()

	local gizmoMode, changed = imgui.combo("Gizmo", { "Move", "Scale" }, GizmoMode)

    if changed then
        CoreTools.SetGizmoMode(gizmoMode)
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

type GizmoDragHandle = (GameObject & EcModel & { Cone: EcMesh, Cylinder: EcMesh })?

local PrevDragMousePos: vector? = nil
local IsSurfaceDragging = false
local IsGizmoDragging = false
local GizmoDragAxis = vector.zero
local GizmoDragHandle: GizmoDragHandle = nil
local GizmoDragStartWorldPoint = nil
local GizmoDragStartObjectPosition = nil
local GizmoDragStartTransform = nil
local GizmoDragStartSize = vector.one
local GizmoDragPlaneNormal = nil
local BeganSurfaceDragAction: number? = nil
local BeganGizmoDragAction: number? = nil
local PrevCamPos = Workspace.SceneCamera.Transform.Translation

local function roundTo(n: number, x: number): number
    return math.round(n / x) * x
end

local function resetGizmo()
    GizmoContainer.Parent = nil
    GizmoPrevTarget = nil
    GizmoPrevTargetTransform = Matrix.identity
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
    local screenPos = Workspace:WorldToScreenPoint(Point) - ViewportRenderPosition

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

    local cam = Workspace.SceneCamera

    local orbitSpeed = math.rad(15) -- degrees/sec
    local dx, dy = 0, 0

    for _, v in GizmoPieceToDirection do
        local tdx, tdy = findDxDyToKeepOnScreen(v / 2 + target.Transform.Translation, ViewportRenderPosition, ViewportRenderSize)

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
        GizmoDragStartObjectPosition = target.Transform.Translation
    end

    -- local axisLocal = assert(GizmoPieceToDirection[GizmoDragHandle])
    -- local rotOnly = target.Transform - target.Transform.Position
    -- local axisWorld = vector.normalize(rotOnly * axisLocal)
    -- GizmoDragPlaneNormal = vector.normalize(vector.cross(axisWorld, cam.Transform.Forward))
end

local function update(Project: Types.Project, EditorPreferences, ViewportRenderPosition: vector, ViewportRenderSize: vector, DeltaTime: number)
    if not Project.Workspace then
        return
    end

    local selections = DeveloperTools:GetExplorerSelections()
    local target: (GameObject & EcTransform)? = selections[1] and selections[1]:AsComponent("Transform")

    if IsSurfaceDragging or IsGizmoDragging then
        if GizmoPrevTarget and (not target or target ~= GizmoPrevTarget) then
            DeveloperTools:SetExplorerSelections({ GizmoPrevTarget })
            target = GizmoPrevTarget
        end
    end

    local tryingToInteract = not PlayerInput.IsMouseSunk and PlayerInput:IsMouseButtonPressed(Enum.MouseButton.Left)

    local cam = Workspace.SceneCamera
    local camPos = cam.Transform.Translation
    local cpos = PlayerInput:GetCursorPosition()

    local mouseVector = Workspace:ScreenPointToVector(cpos, 1000)

    local cameraHit = if not IsSurfaceDragging and not IsGizmoDragging
                        then Workspace:Raycast(camPos, mouseVector, { CoreTools.VisualizationsContainer }, false)
                        else nil
    local camObj = if cameraHit then assert(cameraHit.Object.Parent) :: GameObject & EcTransform else nil
    local camHitDistance = if cameraHit then vector.magnitude(cameraHit.Position - cam.Transform.Translation) else math.huge

    if camObj then
        if camObj.Enabled then
            assert(camObj:HasComponent("Transform"))
            local corresponding = CoreTools.VisualizationTargets[camObj]

            if corresponding and not corresponding:IsDescendantOf(Project.Workspace) then
                warn("Oops")
                camObj:Destroy()
            end
        else
            camObj = nil
        end
    end

    if cameraHit and CoreTools.HoveringOverCamera ~= camObj and camObj then
        local hidden = vector.magnitude(Workspace.SceneCamera.Transform.Translation - camObj.Transform.Translation) < 0.5

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

    if not target or not Workspace.SceneCamera or target == Workspace.SceneCamera then
        resetGizmo()
        return
    end

    if PlayerInput:IsKeyPressed(Enum.Key.F) and not PlayerInput.IsKeyboardSunk then
        local size = target.Transform.Scale
        local pos = target.Transform.Translation - (cam.Transform.Forward * math.max(size.x, size.y, size.z) * 1.5)
        cam.Transform = Matrix.lookAt(pos, target.Transform.Translation)
    end

    local gizmoHit = if tryingToInteract
                        then Workspace:Raycast(camPos, mouseVector, { GizmoContainer }, false)
                        else nil

    if tryingToInteract and not IsGizmoDragging and not gizmoHit then
        local worldHit = Project.Workspace:Raycast(camPos, mouseVector, if IsSurfaceDragging then { target } else {})

        if worldHit and (not camObj or vector.magnitude(worldHit.Position - cam.Transform.Translation) < camHitDistance) and (IsSurfaceDragging or worldHit.Object == target) then
            assert(worldHit)

            if not IsSurfaceDragging then
                PrevDragMousePos = cpos
            end

            if PrevDragMousePos ~= cpos then
                if not BeganSurfaceDragAction then
                    BeganSurfaceDragAction = History:TryBeginAction("SurfaceDrag")
                end

                target.Transform = Matrix.translated(worldHit.Position + worldHit.Normal * target.Transform.Scale / 2)
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
        GizmoContainer.Parent = Workspace
    end

    if not GizmoActive then
        resetGizmo()
        return
    end

    if target ~= GizmoPrevTarget or target.Transform ~= GizmoPrevTargetTransform or cam.Transform.Translation ~= PrevCamPos then
        local scale = vector.one * vector.magnitude(cam.Transform.Translation - target.Transform.Translation) * math.rad(cam.FieldOfView) / 20
        local gtrans = Matrix.translated(target.Transform.Translation) * Matrix.rotated(target.Transform:ExtractAngles()) * Matrix.scaled(vector.clamp(scale, vector.zero, vector.one * 1000))
        GizmoContainer.Transform = gtrans
        PrevCamPos = cam.Transform.Translation
    end

    GizmoPrevTarget = target
    GizmoPrevTargetTransform = target.Transform

    if tryingToInteract then
        if not IsGizmoDragging and gizmoHit and vector.magnitude(gizmoHit.Position - cam.Transform.Translation) < camHitDistance then
            local hit = gizmoHit.Object
            -- TODO luau if we leave this unannotated, even though the hover hint says `Matrix`,
            -- indexing it will result in `never`
            local targetTrans: Matrix = target.Transform

            PrevDragMousePos = cpos

            local axisLocal = assert(GizmoPieceToDirection[hit])
            local rotOnly = targetTrans - targetTrans.Translation
            local axisWorld = vector.normalize(rotOnly * axisLocal)

            GizmoDragAxis = assert(GizmoPieceToDirection[hit])
            GizmoDragHandle = assert(hit.Parent):AsComponent("Model") :: GizmoDragHandle
            assert(GizmoDragHandle)

            GizmoDragStartTransform = targetTrans
            GizmoDragStartObjectPosition = targetTrans.Translation
            GizmoDragStartSize = targetTrans.Scale

            local camForward = cam.Transform.Forward
            GizmoDragPlaneNormal = vector.normalize(vector.cross(axisWorld, camForward))

            GizmoDragStartWorldPoint = rayPlaneIntersect(camPos, mouseVector, targetTrans.Translation, GizmoDragPlaneNormal)

            for _, d in (if GizmoMode == 1 then MoveGizmo else ScaleGizmo):GetDescendants() do
                local mesh = d:AsComponent("Mesh")
                if mesh then
                    mesh.Transparency = 0.8
                end
            end

            GizmoDragHandle.Cylinder.Transparency = 0
            GizmoDragHandle.Cone.Transparency = 0
            IsGizmoDragging = true
        end

    else
        if IsGizmoDragging then
            for _, d in (if GizmoMode == 1 then MoveGizmo else ScaleGizmo):GetDescendants() do
                local mesh = d:AsComponent("Mesh")
                if mesh then
                    mesh.Transparency = if ShadowMeshes[mesh] then 0.5 else 0
                end
            end
        end

        IsGizmoDragging = false

        if BeganGizmoDragAction then
            History:FinishAction(BeganGizmoDragAction)
            BeganGizmoDragAction = nil
        end
    end

    local hit = IsGizmoDragging
                and GizmoDragStartObjectPosition
                and GizmoDragPlaneNormal
                and GizmoDragStartWorldPoint
                and rayPlaneIntersect(camPos, mouseVector, GizmoDragStartObjectPosition, GizmoDragPlaneNormal)

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
            assert(GizmoPieceToDirection[GizmoDragHandle], GizmoDragHandle:GetFullName())
            local adjust = not PlayerInput:IsKeyPressed(Enum.Key.LeftShift)

            local newSize = GizmoDragStartSize + GizmoPieceToDirection[GizmoDragHandle] * (if adjust then amount else amount * 2) * math.sign(delta.x + delta.y + delta.z)

            if newSize.x > 0 and newSize.y > 0 and newSize.z > 0 then
                if adjust then
                    target.Transform = GizmoDragStartTransform * Matrix.translated(move * 0.5)
                end

                target.Transform = target.Transform:WithScale(newSize)
            end
        end

        PrevDragMousePos = cpos

        if EditorPreferences.KeepGizmoInView then
            keepGizmoInView(target, ViewportRenderPosition, ViewportRenderSize, DeltaTime)
        end
    end
end

function CoreTools.Update(Project: Types.Project, EditorPreferences: { read KeepGizmoInView: boolean }, ViewportRenderPosition: vector, ViewportRenderSize: vector, DeltaTime: number)
    debug.zonebegin("CoreTools.Update")

    local success, err = pcall(update, Project, EditorPreferences, ViewportRenderPosition, ViewportRenderSize, DeltaTime)

    debug.zoneend()

    if not success then
        warn(err)
    end
end

return CoreTools
