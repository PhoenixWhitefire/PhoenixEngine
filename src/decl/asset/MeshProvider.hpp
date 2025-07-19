// 16/09/2024
// Loads, saves, and provides Meshes by their Resource ID

#pragma once

#include <functional>
#include <future>

#include "datatype/Mesh.hpp"
#include "render/GpuBuffers.hpp"
#include "Memory.hpp"

class MeshProvider
{
public:
	struct GpuMesh
	{
		void Delete()
		{
			this->VertexArray.Delete();
			this->VertexBuffer.Delete();
			this->ElementBuffer.Delete();
		}

		GpuVertexArray VertexArray;
		GpuVertexBuffer VertexBuffer;
		GpuElementBuffer ElementBuffer;
		uint32_t NumIndices = UINT32_MAX;
		uint32_t SkinningBuffer = UINT32_MAX;
		std::vector<glm::mat4> SkinningData;
	};

	void Shutdown();
	// USE SHUTDOWN!!
	~MeshProvider();

	void Initialize();

	static MeshProvider* Get();

	void FinalizeAsyncLoadedMeshes();

	std::string Serialize(const Mesh&);
	Mesh Deserialize(const std::string_view&, std::string* ErrorMessage);
	// mesh is intentionally copied here, because it must be copied into
	// an internal array (`m_Meshes`) anyway, and may need to be modified,
	// depending on `UploadToGpu`
	uint32_t Assign(Mesh, const std::string_view& InternalName, bool UploadToGpu = true);

	void Save(const Mesh&, const std::string_view& Path);
	void Save(uint32_t, const std::string_view& Path);
	
	uint32_t LoadFromPath(
		const std::string_view& Path,
		bool ShouldLoadAsync = true,
		bool PreserveMeshData = false,
		std::function<void(Mesh&)> PostLoadCallback = nullptr
	);

	Mesh& GetMeshResource(uint32_t);
	GpuMesh& GetGpuMesh(uint32_t);

private:
	void m_CreateAndUploadGpuMesh(Mesh&);
	void m_CreateAndUploadGpuMesh(uint32_t);

	Memory::vector<Mesh, MEMCAT(Mesh)> m_Meshes;
	Memory::unordered_map<std::string, uint32_t, MEMCAT(Mesh)> m_StringToMeshId;

	struct MeshLoadRequest
	{
		std::promise<Mesh>* Promise;
		std::shared_future<Mesh> Future;
		uint32_t ResourceId = UINT32_MAX;
		std::function<void(Mesh&)> PostLoadCallback = nullptr;
	};

	std::vector<MeshLoadRequest> m_LoadingRequests;

	Memory::vector<GpuMesh, MEMCAT(Mesh)> m_GpuMeshes;
};
