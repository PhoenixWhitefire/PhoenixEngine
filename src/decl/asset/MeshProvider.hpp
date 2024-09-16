// 16/09/2024
// Loads, saves, and provides Meshes by their Resource ID

#pragma once

#include<future>
#include"datatype/Mesh.hpp"

class MeshProvider
{
public:
	static MeshProvider* Get();

	void FinalizeAsyncLoadedMeshes();

	std::string Serialize(const Mesh&);
	Mesh Deserialize(const std::string&, bool*);
	void Save(const Mesh&, const std::string& Path);
	void Save(uint32_t, const std::string& Path);
	uint32_t Load(const Mesh&, const std::string& InternalName);
	uint32_t LoadFromPath(const std::string& Path, bool ShouldLoadAsync = true);

	Mesh* GetMeshResource(uint32_t);

	const std::string& GetLastErrorString();

private:
	MeshProvider();

	std::vector<std::promise<Mesh*>*> m_MeshPromises;
	std::vector<std::shared_future<Mesh*>> m_MeshFutures;
	std::unordered_map<std::string, uint32_t> m_StringToMeshId;
	std::unordered_map<uint32_t, Mesh> m_Meshes;
};
