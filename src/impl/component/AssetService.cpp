#include "component/AssetService.hpp"
#include "asset/MaterialManager.hpp"
#include "asset/TextureManager.hpp"
#include "asset/ModelImporter.hpp"
#include "asset/MeshProvider.hpp"
#include "asset/SceneFormat.hpp"
#include "FileRW.hpp"

static void loadMeshDataFromMap(const std::vector<Reflection::GenericValue>& inputs, Mesh& mesh)
{
    const std::unordered_map<Reflection::GenericValue, Reflection::GenericValue>& meshData = inputs[1].AsMap();

    std::span<Reflection::GenericValue> vertices = meshData.at("Vertices").AsArray();
    std::span<Reflection::GenericValue> indices = meshData.at("Indices").AsArray();

    mesh.Vertices.reserve(vertices.size());
    mesh.Indices.reserve(indices.size());

    for (const Reflection::GenericValue& vertexData : vertices)
    {
        std::unordered_map<Reflection::GenericValue, Reflection::GenericValue> vertex = vertexData.AsMap();
        std::span<Reflection::GenericValue> paintData = vertex.at("Paint").AsArray();

        mesh.Vertices.push_back(Vertex{
            .Position = vertex.at("Position").AsVector3(),
            .Normal = vertex.at("Normal").AsVector3(),
            .Paint = glm::vec4(paintData[0].AsDouble(), paintData[1].AsDouble(), paintData[2].AsDouble(), paintData[3].AsDouble()),
            .TextureUV = glm::vec2(vertex.at("UV").AsVector3())
        });
    }

    for (const Reflection::GenericValue& indexData : indices)
    {
        int64_t i = indexData.AsInteger();
        if (i <= 0)
            RAISE_RTF("Got invalid index '{}', must be positive (index into Vertices table)", i);
        else if (i > UINT32_MAX)
            RAISE_RTF("Got invalid index '{}', out of 32-bit unsigned integer range (0..{} inclusive)", i, UINT32_MAX);

        mesh.Indices.push_back((uint32_t)i - 1);
    }
}

static uint8_t readU8(const std::string_view& vec, size_t offset, bool* fileTooSmallPtr)
{
	if (*fileTooSmallPtr || vec.size() < offset + 1)
	{
		*fileTooSmallPtr = true;
		return UINT8_MAX;
	}

	uint8_t u8 = 0;
	memcpy(&u8, vec.data() + offset, sizeof(uint8_t));

	return u8;
}

static uint8_t readU8(const std::string_view& vec, size_t* offset, bool* fileTooSmallPtr)
{
	uint8_t u8 = readU8(vec, *offset, fileTooSmallPtr);
	*offset += 1ull;

	return u8;
}

static uint32_t readU32(const std::string_view& vec, size_t offset, bool* fileTooSmallPtr)
{
	if (*fileTooSmallPtr || vec.size() < offset + 4)
	{
		*fileTooSmallPtr = true;
		return UINT32_MAX;
	}

	uint32_t u32 = 0;
	memcpy(&u32, vec.data() + offset, sizeof(uint32_t));

	return u32;
}

static uint32_t readU32(const std::string_view& vec, size_t* offset, bool* fileTooSmallPtr)
{
	uint32_t u32 = readU32(vec, *offset, fileTooSmallPtr);
	*offset += 4ull;

	return u32;
}

static float readF32(const std::string_view& vec, size_t* offset, bool* fileTooSmallPtr)
{
	if (*fileTooSmallPtr || vec.size() < (*offset) + 4)
	{
		*fileTooSmallPtr = true;
		return FLT_MAX;
	}

	float f32 = 0.f;
	memcpy(&f32, vec.data() + *offset, sizeof(float));

	*offset += 4ull;

	return f32;
}

static void loadMeshDataFromBuffer(const std::vector<Reflection::GenericValue>& inputs, Mesh& mesh)
{
    const std::string_view& buffer = std::string_view(inputs[1].Val.Str, inputs[1].Size);

    if (buffer.size() < sizeof(uint32_t) * 2)
        RAISE_RTF("Expected buffer to have at least {} bytes, got {}", sizeof(uint32_t) * 2, buffer.size());

    bool notEnoughData = false;
    size_t cursor = 0;
    uint32_t numVerts = readU32(buffer, &cursor, &notEnoughData);
    uint32_t numInds = readU32(buffer, &cursor, &notEnoughData);

    if (numInds == 0)
        return;

    mesh.Vertices.reserve(numVerts);
    mesh.Indices.reserve(numInds);

    for (uint32_t vi = 0; vi < numVerts; vi++)
    {
        float px = readF32(buffer, &cursor, &notEnoughData);
        float py = readF32(buffer, &cursor, &notEnoughData);
        float pz = readF32(buffer, &cursor, &notEnoughData);

        if (notEnoughData)
            RAISE_RTF("Reached end of buffer reading positions of vertex {} (offset: {})", vi + 1, cursor);

        float nx = readF32(buffer, &cursor, &notEnoughData);
        float ny = readF32(buffer, &cursor, &notEnoughData);
        float nz = readF32(buffer, &cursor, &notEnoughData);

        if (notEnoughData)
            RAISE_RTF("Reached end of buffer reading normals of vertex {} (offset: {})", vi + 1, cursor);

        float r = (float)readU8(buffer, &cursor, &notEnoughData) / 255.f;
        float g = (float)readU8(buffer, &cursor, &notEnoughData) / 255.f;
        float b = (float)readU8(buffer, &cursor, &notEnoughData) / 255.f;
        float a = (float)readU8(buffer, &cursor, &notEnoughData) / 255.f;

        if (notEnoughData)
            RAISE_RTF("Reached end of buffer reading paint of vertex {} (offset: {})", vi + 1, cursor);

        float u = readF32(buffer, &cursor, &notEnoughData);
        float v = readF32(buffer, &cursor, &notEnoughData);

        if (notEnoughData)
            RAISE_RTF("Reached end of buffer reading UVs of vertex {} (offset: {})", vi + 1, cursor);

        mesh.Vertices.push_back(Vertex{
            .Position = glm::vec3(px, py, pz),
            .Normal = glm::vec3(nx, ny, nz),
            .Paint = glm::vec4(r, g, b, a),
            .TextureUV = glm::vec2(u, v)
        });
    }

    for (uint32_t ii = 0; ii < numInds; ii++)
    {
        uint32_t u32 = readU32(buffer, &cursor, &notEnoughData);
        if (notEnoughData)
            RAISE_RTF("Reached of buffer reading index {} (offset: {})", ii + 1, cursor);

        if (u32 == 0)
            RAISE_RTF("Expected 1-based indices, but index {} is 0", ii + 1);

        mesh.Indices.push_back(u32 - 1);
    }
}

const Reflection::StaticMethodMap& AssetServiceComponentManager::GetMethods()
{
    static const Reflection::StaticMethodMap methods = {
        { "GetMeshData", Reflection::MethodDescriptor{
            { Reflection::ValueType::String },
            { Reflection::ValueType::Map },
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                MeshProvider* meshProvider = MeshProvider::Get();

	            uint32_t meshId = meshProvider->LoadFromPath(std::string(inputs[0].AsStringView()), false, true);
	            Mesh& mesh = meshProvider->GetMeshResource(meshId);

                std::unordered_map<Reflection::GenericValue, Reflection::GenericValue> meshData;
                std::vector<Reflection::GenericValue> indices;
                indices.reserve(mesh.Indices.size());

                for (uint32_t ind : mesh.Indices)
                    indices.emplace_back((int64_t)ind + 1);

                meshData["Indices"] = indices;

                std::vector<Reflection::GenericValue> vertices;
                vertices.reserve(mesh.Vertices.size());

                for (const Vertex& vert : mesh.Vertices)
                {
                    std::unordered_map<Reflection::GenericValue, Reflection::GenericValue> vertex;
                    vertex.reserve(4);

                    vertex["Position"] = vert.Position;
                    vertex["Normal"] = vert.Normal;
                    vertex["UV"] = vert.TextureUV;

                    std::vector<Reflection::GenericValue> paint;
                    paint.resize(4);
                    paint[0] = vert.Paint.x;
                    paint[1] = vert.Paint.y;
                    paint[2] = vert.Paint.z;
                    paint[3] = vert.Paint.w;

                    vertex["Paint"] = paint;

                    vertices.emplace_back(vertex);
                }

                meshData["Vertices"] = vertices;

                return { Reflection::GenericValue(meshData) };
            }
        } },

        { "SetMeshData", Reflection::MethodDescriptor{
            { Reflection::ValueType::String, Reflection::ValueType::Any },
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                const std::string& name = std::string(inputs[0].AsStringView());

	            Mesh mesh;
	            mesh.MeshDataPreserved = true;

                if (inputs[1].Type == Reflection::ValueType::Map)
                    loadMeshDataFromMap(inputs, mesh);
                else if (inputs[1].Type == Reflection::ValueType::Buffer)
                    loadMeshDataFromBuffer(inputs, mesh);
                else
                    RAISE_RTF("Expected mesh data to be buffer or map (table), got '{}' ()", inputs[1].ToString(), Reflection::TypeAsString(inputs[1].Type));

                MeshProvider::Get()->Assign(mesh, name);
                return {};
            }
        } },

        { "SaveMesh", Reflection::MethodDescriptor{
            { Reflection::ValueType::String, Reflection::ValueType::String },
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                const std::string& internalName = std::string(inputs[0].AsStringView());
	            const std::string& savePath = std::string(inputs[0].AsStringView());

                MeshProvider* meshProv = MeshProvider::Get();
	            meshProv->Save(meshProv->LoadFromPath(internalName), savePath);

                return {};
            }
        } },

        { "ImportModel", Reflection::MethodDescriptor{
            { Reflection::ValueType::String },
            { Reflection::ValueType::GameObject },
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                const std::string& path = std::string(inputs[0].AsStringView());
	            std::vector<ObjectRef> loaded = ModelLoader(path, PHX_GAMEOBJECT_NULL_ID).LoadedObjs;

                return { loaded.at(0)->ToGenericValue() };
            }
        } },

        { "LoadScene", Reflection::MethodDescriptor{
            { Reflection::ValueType::String },
            { REFLECTION_OPTIONAL(Array), REFLECTION_OPTIONAL(String) },
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                bool readSuccess = true;
                std::string contents = FileRW::ReadFile(inputs[0].AsString(), &readSuccess);

                if (!readSuccess)
                    return { Reflection::GenericValue::Null(), contents };

                std::vector<ObjectRef> objects = SceneFormat::Deserialize(contents, &readSuccess);

                if (!readSuccess)
                    return { Reflection::GenericValue::Null(), SceneFormat::GetLastErrorString() };

                std::vector<Reflection::GenericValue> returnArray;
                returnArray.reserve(objects.size());

                for (const ObjectRef& obj : objects)
                    returnArray.push_back(obj->ToGenericValue());

                return { Reflection::GenericValue(returnArray), "" };
            }
        } },

        { "SaveScene", Reflection::MethodDescriptor{
            { Reflection::ValueType::Array, Reflection::ValueType::String },
            { Reflection::ValueType::Boolean, REFLECTION_OPTIONAL(String) },
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                std::vector<GameObject*> objects;
                objects.reserve(inputs[0].Size);

                for (const Reflection::GenericValue& gv : inputs[0].AsArray())
                    objects.push_back(GameObjectManager::Get()->FromGenericValue(gv));

                std::string ser = SceneFormat::Serialize(objects, inputs[1].AsString());

                std::string error;
                bool writeSuccess = FileRW::WriteFileCreateDirectories(inputs[1].AsString(), ser, &error);

                if (!writeSuccess)
                    return { false, error };
                else
                    return { true, "" };
            }
        } },

        { "QueueLoadTexture", Reflection::MethodDescriptor{
            { Reflection::ValueType::String, Reflection::ValueType::Boolean, Reflection::ValueType::Boolean },
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                TextureManager* texManager = TextureManager::Get();
                texManager->LoadFromPath(inputs[0].AsString(), true, inputs[1].AsBoolean(), !inputs[2].AsBoolean());

                return {};
            }
        } },

        { "UnloadTexture", Reflection::MethodDescriptor{
            { Reflection::ValueType::String },
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                TextureManager* texManager = TextureManager::Get();
                uint32_t resId = texManager->LoadFromPath(inputs[0].AsString(), false);
                texManager->UnloadTexture(resId);

                return {};
            }
        } },

        { "UnloadMesh", Reflection::MethodDescriptor{
            { Reflection::ValueType::String },
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                MeshProvider* meshProv = MeshProvider::Get();
                meshProv->UnloadMesh(inputs[0].AsString());

                return {};
            }
        } },

        { "UnloadMaterial", Reflection::MethodDescriptor{
            { Reflection::ValueType::String },
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                MaterialManager* materialManager = MaterialManager::Get();
                materialManager->UnloadMaterial(inputs[0].AsString());

                return {};
            }
        } },
    };

    return methods;
}
