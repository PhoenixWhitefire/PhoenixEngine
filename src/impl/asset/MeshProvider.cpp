#include <iostream>
#include <chrono>
#include <nljson.hpp>
#include <glad/gl.h>
#include <glm/vec4.hpp>

#include "asset/MeshProvider.hpp"
#include "asset/PrimitiveMeshes.hpp"
#include "render/GpuBuffers.hpp"
#include "FileRW.hpp"
#include "Debug.hpp"

#define MESHPROVIDER_ERROR(err) { s_ErrorString = err; *SuccessPtr = false; return {}; }

static std::string s_ErrorString = "No error";

static uint32_t readU32(const std::vector<int8_t>& vec, size_t offset, bool* fileTooSmallPtr)
{
	if (*fileTooSmallPtr || vec.size() - 1 < offset + 3)
	{
		*fileTooSmallPtr = true;
		return 0;
	}

	uint32_t u32{};
	std::memcpy(&u32, &vec.at(offset), 4);

	return u32;
}

static uint32_t readU32(const std::vector<int8_t>& vec, size_t* offset, bool* fileTooSmallPtr)
{
	if (*fileTooSmallPtr || vec.size() - 1 < *offset + 4)
	{
		*fileTooSmallPtr = true;
		return 0;
	}

	uint32_t u32{};
	std::memcpy(&u32, &vec.at(*offset), 4);
	*offset += 4ull;

	return u32;
}

static float readF32(const std::vector<int8_t>& vec, size_t* offset, bool* fileTooSmallPtr)
{
	if (*fileTooSmallPtr || vec.size() - 1 < *offset + 4)
	{
		*fileTooSmallPtr = true;
		return 0.f;
	}

	float f32{};
	std::memcpy(&f32, &vec.at(*offset), 4);
	*offset += 4ull;

	return f32;
}

static void writeU32(std::string& vec, uint32_t v)
{
	vec.push_back(*(int8_t*)&v);
	vec.push_back(*((int8_t*)&v + 1ull));
	vec.push_back(*((int8_t*)&v + 2ull));
	vec.push_back(*((int8_t*)&v + 3ull));
}

static void writeF32(std::string& vec, float v)
{
	uint32_t u{};
	std::memcpy(&u, &v, 4ull);
	writeU32(vec, u);
}

static float getVersion(const std::string& MapFileContents)
{
	size_t matchLocation = MapFileContents.find("#Version");

	float version = 0.f;

	if (matchLocation != std::string::npos)
	{
		std::string subStr = MapFileContents.substr(matchLocation + 9, 4);
		version = std::stof(subStr);
	}

	return version;
}

static Mesh loadMeshVersion1(const std::string& FileContents, bool* SuccessPtr)
{
	size_t jsonStartLoc = FileContents.find_first_of("{");
	std::string jsonFileContents = FileContents.substr(jsonStartLoc);
	nlohmann::json json = nlohmann::json::parse(jsonFileContents);

	Mesh mesh;
	mesh.Vertices.reserve(json["Vertices"].size());
	mesh.Indices.reserve(json["Indices"].size());

	size_t vertexIndex = 0;

	for (nlohmann::json vertexDesc : json["Vertices"])
	{
		if (vertexDesc.size() % 11 != 0)
			MESHPROVIDER_ERROR("(V1) Vertex #" + std::to_string(vertexIndex) + " does not have 11 elements");

		Vertex vertex =
		{
			glm::vec3(vertexDesc[0], vertexDesc[1], vertexDesc[2]),
			glm::vec3(vertexDesc[3], vertexDesc[4], vertexDesc[5]),
			glm::vec3(vertexDesc[6], vertexDesc[7], vertexDesc[8]),
			glm::vec2(vertexDesc[9], vertexDesc[10])
		};

		mesh.Vertices.emplace_back(
			glm::vec3(vertexDesc[0], vertexDesc[1], vertexDesc[2]),
			glm::vec3(vertexDesc[3], vertexDesc[4], vertexDesc[5]),
			glm::vec3(vertexDesc[6], vertexDesc[7], vertexDesc[8]),
			glm::vec2(vertexDesc[9], vertexDesc[10])
		);

		vertexIndex += 1;
	}

	for (uint32_t index : json["Indices"])
		mesh.Indices.push_back(index);

	return mesh;
}

static Mesh loadMeshVersion2(const std::string& FileContents, bool* SuccessPtr)
{
	size_t binaryStartLoc = FileContents.find_first_of('$');

	if (binaryStartLoc == std::string::npos)
		MESHPROVIDER_ERROR("File did not contain a binary data begin symbol ('$')");

	std::string binaryContents = FileContents.substr(binaryStartLoc + 1);

	std::vector<int8_t> data( binaryContents.begin(), binaryContents.end() );

	if (data.size() < 12)
		MESHPROVIDER_ERROR("File cannot contain header as binary data is smaller than 12 bytes");

	size_t headerPtr{};
	bool fileTooSmallError = false;

	uint32_t vertexMeta = readU32(data, &headerPtr, &fileTooSmallError);
	uint32_t numVerts = readU32(data, &headerPtr, &fileTooSmallError);
	uint32_t numIndices = readU32(data, &headerPtr, &fileTooSmallError);

	if (fileTooSmallError)
		MESHPROVIDER_ERROR("This should have been caught earlier, but header was smaller than 12 bytes");

	bool hasVertexOpacity = vertexMeta & 0b00000001;
	bool hasVertexColor   = vertexMeta & 0b00000010;
	bool hasVertexNormal  = vertexMeta & 0b00000100;

	glm::vec3 uniformVertexNormal{};
	glm::vec4 uniformVertexRGBA{ 1.f, 1.f, 1.f, 1.f };

	if (!hasVertexNormal)
		uniformVertexNormal = glm::vec3
		{
			readF32(data, &headerPtr, &fileTooSmallError),
			readF32(data, &headerPtr, &fileTooSmallError),
			readF32(data, &headerPtr, &fileTooSmallError)
		};

	if (!hasVertexColor)
		uniformVertexRGBA = glm::vec4
		{
			readF32(data, &headerPtr, &fileTooSmallError),
			readF32(data, &headerPtr, &fileTooSmallError),
			readF32(data, &headerPtr, &fileTooSmallError),
			1.f
		};

	if (!hasVertexOpacity)
		uniformVertexRGBA.w = readF32(data, &headerPtr, &fileTooSmallError);

	if (fileTooSmallError)
		MESHPROVIDER_ERROR("File ended during preamble");

	// Px, Py, Pz, (Nx, Ny, Nz), (R, G, B), (A), Tu, Tv
	size_t floatsPerVertex = 3ull + (hasVertexNormal ? 3 : 0) + (hasVertexColor ? 3 : 0) + (hasVertexOpacity ? 1 : 0) + 2;
	size_t bytesPerVertex = floatsPerVertex * 4ull;

	size_t totalExpectedDataSize = bytesPerVertex * numVerts + numIndices * 4ull;
	size_t actualDataSize = data.size() - headerPtr;

	if (actualDataSize < totalExpectedDataSize)
		MESHPROVIDER_ERROR(std::vformat(
			"Binary section of File was expected to be {} bytes, but was {} instead (smaller)",
			std::make_format_args(totalExpectedDataSize, actualDataSize)
		));

	Mesh mesh{};
	mesh.Vertices.reserve(numVerts);
	mesh.Indices.reserve(numIndices);

	for (uint32_t vertexIndex = 0; vertexIndex < numVerts; vertexIndex++)
	{
		size_t vertBytePtr = vertexIndex * bytesPerVertex + headerPtr;

		float px = readF32(data, &vertBytePtr, &fileTooSmallError);
		float py = readF32(data, &vertBytePtr, &fileTooSmallError);
		float pz = readF32(data, &vertBytePtr, &fileTooSmallError);

		float nx = hasVertexNormal ? readF32(data, &vertBytePtr, &fileTooSmallError) : uniformVertexNormal.x;
		float ny = hasVertexNormal ? readF32(data, &vertBytePtr, &fileTooSmallError) : uniformVertexNormal.y;
		float nz = hasVertexNormal ? readF32(data, &vertBytePtr, &fileTooSmallError) : uniformVertexNormal.z;

		float r = hasVertexColor ? readF32(data, &vertBytePtr, &fileTooSmallError) : uniformVertexRGBA.x;
		float g = hasVertexColor ? readF32(data, &vertBytePtr, &fileTooSmallError) : uniformVertexRGBA.y;
		float b = hasVertexColor ? readF32(data, &vertBytePtr, &fileTooSmallError) : uniformVertexRGBA.z;
		float a = hasVertexOpacity ? readF32(data, &vertBytePtr, &fileTooSmallError) : uniformVertexRGBA.w;

		float u = readF32(data, &vertBytePtr, &fileTooSmallError);
		float v = readF32(data, &vertBytePtr, &fileTooSmallError);

		mesh.Vertices.emplace_back(
			glm::vec3{ px, py, pz },
			glm::vec3{ nx, ny, nz },
			glm::vec3{ r, g, b },
			glm::vec2{ u, v }
		);
	}

	for (uint32_t indexIndex = 0; indexIndex < numIndices; indexIndex++)
		mesh.Indices.push_back(readU32(data, numVerts * bytesPerVertex + (indexIndex * 4ull) + headerPtr, &fileTooSmallError));

	if (fileTooSmallError)
	{
		*SuccessPtr = false;
		s_ErrorString = "Binary section of File was too small, and the loader reached the end of it while reading some data";
		// return the mesh because whatever bro
	}

	return mesh;
}

MeshProvider::MeshProvider()
{
	m_CreateAndUploadGpuMesh(this->Assign(PrimitiveMeshes::Cube(), "!Cube"));
	m_CreateAndUploadGpuMesh(this->Assign(PrimitiveMeshes::Quad(), "!Quad"));
}

MeshProvider::~MeshProvider()
{
	for (size_t promIndex = 0; promIndex < m_MeshPromises.size(); promIndex++)
	{
		std::promise<Mesh>* prom = m_MeshPromises[promIndex];
		std::shared_future<Mesh>& future = m_MeshFutures[promIndex];

		// we don't want our async threads accessing deleted promises
		future.wait();

		delete prom;
	}

	for (GpuMesh& gpuMesh : m_GpuMeshes)
		gpuMesh.Delete();

	m_Meshes.clear();
	m_StringToMeshId.clear();
	m_MeshFutures.clear();
	m_MeshPromises.clear();
	m_MeshPromiseResourceIds.clear();
	m_GpuMeshes.clear();
}

static bool s_DidShutdown = false;

MeshProvider* MeshProvider::Get()
{
	if (s_DidShutdown)
		throw("Tried to ::Get MeshProvider after it was ::Shutdown");

	static MeshProvider inst;
	return &inst;
}

void MeshProvider::Shutdown()
{
	//delete Get();
	s_DidShutdown = true;
}

std::string MeshProvider::Serialize(const Mesh& mesh)
{
	std::string contents = "Generated by Phoenix Engine\n#Version 2.00\n#Asset Mesh\n";

	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	std::chrono::year_month_day ymd = std::chrono::floor<std::chrono::days>(now);

	contents += "#Date "
				+ std::to_string((uint32_t)ymd.day()) + "-"
				+ std::to_string((uint32_t)ymd.month()) + "-"
				+ std::to_string((int32_t)ymd.year()) + "\n\n"
				+ "$";

	bool hasPerVertexColor = false;
	glm::vec3 uniformVertexCol = mesh.Vertices.at(0).Color;

	for (const Vertex& v : mesh.Vertices)
		if (v.Color != uniformVertexCol)
		{
			hasPerVertexColor = true;
			break;
		}

	// w/ PVC: Px, Py, Pz, Nx, Ny, Nz, R, G, B, A, Tu, Tv
	// w/o PV: Px, Py, Pz, Nx, Ny, Nz, Tu, Tv
	size_t floatsPerVertex = hasPerVertexColor ? 12ull : 8ull;

	// 12b header + 4b padding
	contents.reserve(16ull + mesh.Vertices.size() * (floatsPerVertex * 4ull) + mesh.Indices.size() * 4ull + contents.size());

	// per-vertex normal, color and opacity flags
	writeU32(contents, 0b00000100 + (hasPerVertexColor ? 0b00000011 : 0));
	writeU32(contents, static_cast<uint32_t>(mesh.Vertices.size()));
	writeU32(contents, static_cast<uint32_t>(mesh.Indices.size()));

	if (!hasPerVertexColor)
	{
		writeF32(contents, uniformVertexCol.x);
		writeF32(contents, uniformVertexCol.y);
		writeF32(contents, uniformVertexCol.z);
		writeF32(contents, 1.f);
	}

	for (const Vertex& v : mesh.Vertices)
	{
		writeF32(contents, v.Position.x);
		writeF32(contents, v.Position.y);
		writeF32(contents, v.Position.z);

		writeF32(contents, v.Normal.x);
		writeF32(contents, v.Normal.y);
		writeF32(contents, v.Normal.z);

		if (hasPerVertexColor)
		{
			writeF32(contents, v.Color.x);
			writeF32(contents, v.Color.y);
			writeF32(contents, v.Color.z);
			writeF32(contents, 1.f);
		}

		writeF32(contents, v.TextureUV.x);
		writeF32(contents, v.TextureUV.y);
	}

	for (uint32_t i : mesh.Indices)
		writeU32(contents, i);

	return contents;
}

Mesh MeshProvider::Deserialize(const std::string& Contents, bool* SuccessPtr)
{
	*SuccessPtr = true;

	if (Contents.empty())
		MESHPROVIDER_ERROR("Mesh file is empty");

	float version = getVersion(Contents);

	if (version == 0.f)
		MESHPROVIDER_ERROR("No Version header");

	if (version >= 1.f && version < 2.f)
		return loadMeshVersion1(Contents, SuccessPtr);

	if (version >= 2.f && version < 3.f)
		return loadMeshVersion2(Contents, SuccessPtr);

	MESHPROVIDER_ERROR(std::string("Unrecognized mesh version - ") + std::to_string(version));
}

void MeshProvider::Save(const Mesh& mesh, const std::string& Path)
{
	std::string contents = this->Serialize(mesh);
	FileRW::WriteFileCreateDirectories(Path, contents, true);
}

void MeshProvider::Save(uint32_t Id, const std::string& Path)
{
	this->Save(m_Meshes.at(Id), Path);
}

uint32_t MeshProvider::Assign(const Mesh& mesh, const std::string& InternalName)
{
	uint32_t assignedId = static_cast<uint32_t>(m_Meshes.size());

	auto prevPair = m_StringToMeshId.find(InternalName);

	if (prevPair != m_StringToMeshId.end())
	{
		// overwrite the pre-existing mesh
		m_Meshes[prevPair->second] = mesh;
		assignedId = prevPair->second;
	}
	else
	{
		m_StringToMeshId.insert(std::pair(InternalName, assignedId));
		m_Meshes.push_back(mesh);
	}

	return assignedId;
}

uint32_t MeshProvider::LoadFromPath(const std::string& Path, bool ShouldLoadAsync, bool PreserveMeshData)
{
	auto meshIt = m_StringToMeshId.find(Path);

	if (meshIt != m_StringToMeshId.end())
	{
		Mesh& mesh = this->GetMeshResource(meshIt->second);

		// reload the mesh to keep the mesh data CPU side
		// primarily for `mesh_get` for Luau
		// 02/11/2024
		// no i don't feel like actually testing this to make sure it works
		if (PreserveMeshData && !mesh.MeshDataPreserved)
		{
			bool meshLoaded = false;

			for (uint32_t asyncMeshIndex = 0; asyncMeshIndex < m_MeshPromiseResourceIds.size(); asyncMeshIndex++)
			{
				if (m_MeshPromiseResourceIds[asyncMeshIndex] != meshIt->second)
					continue;

				m_Meshes[meshIt->second].MeshDataPreserved = true;

				m_MeshFutures[asyncMeshIndex].wait();
				this->FinalizeAsyncLoadedMeshes();
				meshLoaded = true;
			}

			if (!meshLoaded)
			{
				meshIt = m_StringToMeshId.end();
				ShouldLoadAsync = false;
			}
		}
	}

	if (meshIt == m_StringToMeshId.end())
	{
		bool success = true;

		std::string contents = FileRW::ReadFile(Path, &success);

		if (!success)
		{
			Debug::Log(std::vformat(
				"MeshProvider Failed to load mesh '{}': Invalid path/File could not be opened",
				std::make_format_args(Path)
			));

			return this->Assign(Mesh{}, Path);
		}
		else
		{
			if (ShouldLoadAsync)
			{
				std::promise<Mesh>* promise = new std::promise<Mesh>;

				uint32_t resourceId = this->Assign(Mesh{}, Path);
				m_Meshes.at(resourceId).MeshDataPreserved = PreserveMeshData;

				std::thread(
					[promise, resourceId, this, Path, contents]()
					{
						bool deserialized = true;
						Mesh loadedMesh = this->Deserialize(contents, &deserialized);

						if (!deserialized)
							Debug::Log(std::vformat(
								"MeshProvider failed to load mesh '{}' asynchronously: {}",
								std::make_format_args(Path, s_ErrorString)
							));

						promise->set_value_at_thread_exit(loadedMesh);
					}
				).detach();

				m_MeshPromises.push_back(promise);
				m_MeshFutures.push_back(promise->get_future().share());
				m_MeshPromiseResourceIds.push_back(resourceId);

				return resourceId;
			}
			else
			{
				Mesh mesh = this->Deserialize(contents, &success);
				mesh.MeshDataPreserved = PreserveMeshData;

				if (!success)
					Debug::Log(std::vformat(
						"MeshProvider failed to load mesh '{}': {}",
						std::make_format_args(Path, s_ErrorString)
					));

				m_CreateAndUploadGpuMesh(mesh);

				return this->Assign(mesh, Path);
			}
		}
	}
	else
		return meshIt->second;
}

Mesh& MeshProvider::GetMeshResource(uint32_t Id)
{
	return m_Meshes.at(Id);
}

MeshProvider::GpuMesh& MeshProvider::GetGpuMesh(uint32_t Id)
{
	return m_GpuMeshes.at(Id);
}

void MeshProvider::FinalizeAsyncLoadedMeshes()
{
	size_t numMeshPromises = m_MeshPromises.size();
	size_t numMeshFutures = m_MeshFutures.size();
	size_t numMeshResourceIds = m_MeshPromiseResourceIds.size();

	if (numMeshPromises != numMeshFutures || numMeshFutures != numMeshResourceIds)
	{
		Debug::Log(std::vformat(
			"FinalizeAsyncLoadedMeshes had {} promises, {} futures and {} resource IDs, cannot proceed safely",
			std::make_format_args(numMeshPromises, numMeshFutures, numMeshResourceIds)
		));
		return;
	}

	for (size_t promiseIndex = 0; promiseIndex < m_MeshPromises.size(); promiseIndex++)
	{
		std::promise<Mesh>* promise = m_MeshPromises[promiseIndex];
		std::shared_future<Mesh>& f = m_MeshFutures[promiseIndex];

		if (!f.valid() || f.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			continue;

		const Mesh& loadedMesh = f.get();
		uint32_t resourceId = m_MeshPromiseResourceIds[promiseIndex];

		Mesh& mesh = m_Meshes.at(resourceId);

		mesh.Vertices = loadedMesh.Vertices;
		mesh.Indices = loadedMesh.Indices;

		m_CreateAndUploadGpuMesh(mesh);

		m_MeshPromises.erase(m_MeshPromises.begin() + promiseIndex);
		m_MeshFutures.erase(m_MeshFutures.begin() + promiseIndex);
		m_MeshPromiseResourceIds.erase(m_MeshPromiseResourceIds.begin() + promiseIndex);

		delete promise;
	}
}

const std::string& MeshProvider::GetLastErrorString()
{
	return s_ErrorString;
}

void MeshProvider::m_CreateAndUploadGpuMesh(Mesh& mesh)
{
	m_GpuMeshes.emplace_back();

	MeshProvider::GpuMesh& gpuMesh = m_GpuMeshes.back();

	gpuMesh.VertexArray.Initialize();
	gpuMesh.VertexBuffer.Initialize();
	gpuMesh.ElementBuffer.Initialize();

	GpuVertexArray& vao = gpuMesh.VertexArray;
	GpuVertexBuffer& vbo = gpuMesh.VertexBuffer;
	GpuElementBuffer& ebo = gpuMesh.ElementBuffer;

	vao.Bind();

	vao.LinkAttrib(vbo, 0, 3, GL_FLOAT, sizeof(Vertex), (void*)0);
	vao.LinkAttrib(vbo, 1, 3, GL_FLOAT, sizeof(Vertex), (void*)(3 * sizeof(float)));
	vao.LinkAttrib(vbo, 2, 3, GL_FLOAT, sizeof(Vertex), (void*)(6 * sizeof(float)));
	vao.LinkAttrib(vbo, 3, 2, GL_FLOAT, sizeof(Vertex), (void*)(9 * sizeof(float)));

	vbo.SetBufferData(mesh.Vertices, BufferUsageHint::Static);
	ebo.SetBufferData(mesh.Indices, BufferUsageHint::Static);

	vbo.Unbind();
	ebo.Unbind();
	vao.Unbind();

	gpuMesh.NumIndices = static_cast<uint32_t>(mesh.Indices.size());

	if (!mesh.MeshDataPreserved)
	{
		mesh.Vertices.clear();
		mesh.Indices.clear();
	}

	mesh.GpuId = static_cast<uint32_t>(m_GpuMeshes.size() - 1);
}

void MeshProvider::m_CreateAndUploadGpuMesh(uint32_t MeshId)
{
	m_CreateAndUploadGpuMesh(m_Meshes.at(MeshId));
}
