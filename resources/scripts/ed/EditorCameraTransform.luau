-- SaveEditorCameraTransform.luau, 07/06/2026
local EditorCameraTransform = {}

local json = phoenix.json
local fs = phoenix.fs

type SavedEditorCamera = {
    FieldOfView: number,
    Position: {
        X: number,
        Y: number,
        Z: number
    },
    Rotation: {
        X: number,
        Y: number,
        Z: number
    }
}

function EditorCameraTransform.Save(EditorCam: EcCamera & EcTransform)
    local position = EditorCam.Transform.Translation
    local rotation = EditorCam.Transform:ExtractAngles()

    local savedCamTrans: SavedEditorCamera = {
        FieldOfView = EditorCam.FieldOfView,
        Position = {
            X = position.x,
            Y = position.y,
            Z = position.z
        },
        Rotation = {
            X = rotation.x,
            Y = rotation.y,
            Z = rotation.z
        }
    }

    local editordata = {}
    if fs.isfile("@project/phoenix.editor") then
        editordata = json.parse(assert(fs.read("@project/phoenix.editor"))) :: any
    end

    editordata.SavedEditorCamera = savedCamTrans
    fs.write("@project/phoenix.editor", json.encode(editordata))
end

function EditorCameraTransform.Load(EditorCam: EcCamera & EcTransform)
    local savedCamTrans: SavedEditorCamera = {
        FieldOfView = 90,
        Position = {
            X = 0,
            Y = 0,
            Z = 0
        },
        Rotation = {
            X = 0,
            Y = 0,
            Z = 0
        }
    }

    if fs.isfile("@project/phoenix.editor") then
        local editordata = json.parse(assert(fs.read("@project/phoenix.editor"))) :: any
        savedCamTrans = (editordata.SavedEditorCamera :: SavedEditorCamera) or savedCamTrans

        savedCamTrans.FieldOfView = savedCamTrans.FieldOfView or 90

        savedCamTrans.Position = savedCamTrans.Position or { X = 0, Y = 0, Z = 0 }
        savedCamTrans.Position.X = savedCamTrans.Position.X or 0
        savedCamTrans.Position.Y = savedCamTrans.Position.Y or 0
        savedCamTrans.Position.Z = savedCamTrans.Position.Z or 0

        savedCamTrans.Rotation = savedCamTrans.Rotation or { X = 0, Y = 0, Z = 0 }
        savedCamTrans.Rotation.X = savedCamTrans.Rotation.X or 0
        savedCamTrans.Rotation.Y = savedCamTrans.Rotation.Y or 0
        savedCamTrans.Rotation.Z = savedCamTrans.Rotation.Z or 0
    end

    EditorCam.FieldOfView = savedCamTrans.FieldOfView
    EditorCam.Transform = Matrix.translated(savedCamTrans.Position.X, savedCamTrans.Position.Y, savedCamTrans.Position.Z)
                            * Matrix.rotated(savedCamTrans.Rotation.X, savedCamTrans.Rotation.Y, savedCamTrans.Rotation.Z)
end

return EditorCameraTransform
