--!strict

local CoreTools = {}

function CoreTools.RenderControls(TabButtonControl)
    if TabButtonControl("Gizmo", "@etextures/editor-icons/Transform.png", true, MeshGizmoActive) then
		MeshGizmoActive = not MeshGizmoActive
	end

	imgui.sameline()

	GizmoMode = imgui.combo("Gizmo", { "Move", "Scale" }, GizmoMode)
end

function CoreTools.Update()

end

return CoreTools
