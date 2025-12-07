#include "component/AssetService.hpp"
#include "asset/ModelImporter.hpp"
#include "asset/MeshProvider.hpp"

class AssetServiceManager : public ComponentManager<EcAssetService>
{
public:
    const Reflection::StaticMethodMap& GetMethods() override
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
                        indices.emplace_back(ind);

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
                        paint.reserve(4);
                        paint[0] = vert.Paint.x;
                        paint[1] = vert.Paint.y;
                        paint[2] = vert.Paint.z;
                        paint[3] = vert.Paint.w;

                        vertex["Paint"] = paint;

                        vertices.emplace_back(vertex);
                    }

                    meshData["Vertices"] = vertices;

                    return { meshData };
                }
            } },

            { "SetMeshData", Reflection::MethodDescriptor{
                { Reflection::ValueType::String, Reflection::ValueType::Map },
                {},
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
                {
                    const std::string& name = std::string(inputs[0].AsStringView());
                    const std::unordered_map<Reflection::GenericValue, Reflection::GenericValue>& meshData = inputs[1].AsMap();

                    std::span<Reflection::GenericValue> vertices = meshData.at("Vertices").AsArray();
                    std::span<Reflection::GenericValue> indices = meshData.at("Indices").AsArray();

	                Mesh mesh;
	                mesh.MeshDataPreserved = true;
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
                            .TextureUV = vertex.at("UV").AsVector2()
                        });
                    }

                    for (const Reflection::GenericValue& indexData : indices)
                        mesh.Indices.push_back((uint32_t)indexData.AsInteger());

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
            } }
        };

        return methods;
    }
};

static AssetServiceManager Instance;
