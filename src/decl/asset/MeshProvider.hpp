// 16/09/2024
// Loads, saves, and provides Meshes by their Resource ID

#pragma once

#include <future>

#include "datatype/Mesh.hpp"
#include "render/GpuBuffers.hpp"

class MeshProvider
{
public:
	struct GpuMesh
	{
		GpuVertexArray* VertexArray = nullptr;
		GpuVertexBuffer* VertexBuffer = nullptr;
		GpuElementBuffer* ElementBuffer = nullptr;
		uint32_t NumIndices{};
	};

	static MeshProvider* Get();
	static void Shutdown();

	void FinalizeAsyncLoadedMeshes();

	std::string Serialize(const Mesh&);
	Mesh Deserialize(const std::string&, bool*);
	uint32_t Assign(const Mesh&, const std::string& InternalName);

	void Save(const Mesh&, const std::string& Path);
	void Save(uint32_t, const std::string& Path);
	
	uint32_t LoadFromPath(const std::string& Path, bool ShouldLoadAsync = true, bool PreserveMeshData = false);

	Mesh& GetMeshResource(uint32_t);
	GpuMesh& GetGpuMesh(uint32_t);

	const std::string& GetLastErrorString();

private:
	MeshProvider();
	~MeshProvider();

	void m_CreateAndUploadGpuMesh(Mesh&);
	void m_CreateAndUploadGpuMesh(uint32_t);

	std::vector<Mesh> m_Meshes;
	std::unordered_map<std::string, uint32_t> m_StringToMeshId;

	std::vector<std::promise<Mesh>*> m_MeshPromises;
	std::vector<std::shared_future<Mesh>> m_MeshFutures;
	std::vector<uint32_t> m_MeshPromiseResourceIds;

	std::vector<GpuMesh> m_GpuMeshes;
};
