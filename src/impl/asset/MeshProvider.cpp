#include<iostream>
#include<iomanip>
#include<ctime>
#include<sstream>
#include<nljson.hpp>

#include"asset/MeshProvider.hpp"
#include"asset/PrimitiveMeshes.hpp"
#include"FileRW.hpp"
#include"Debug.hpp"

#define MESHPROVIDER_ERROR(err) s_ErrorString = err; *SuccessPtr = false

static std::string s_ErrorString = "No error";

static float getVersion(std::string const& MapFileContents)
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

MeshProvider::MeshProvider()
{
	this->Assign(PrimitiveMeshes::Cube(), "!Cube");
}

MeshProvider* MeshProvider::Get()
{
	static MeshProvider instance;
	return &instance;
}

std::string MeshProvider::Serialize(const Mesh& mesh)
{
	std::string contents = "Generated by Phoenix Engine\n#Version 1.00\n#Asset Mesh\n";

	time_t t = std::time(nullptr);
	tm tm = *std::localtime(&t);

	std::ostringstream oss;
	oss << std::put_time(&tm, "%d-%m-%Y");
	std::string dateTimeStr = oss.str();

	contents += "#Date " + dateTimeStr + "\n\n";

	nlohmann::json json;
	nlohmann::json vertSon;
	nlohmann::json indSon;

	for (size_t index = 0; index < mesh.Vertices.size(); index++)
	{
		const Vertex& vert = mesh.Vertices[index];

		vertSon[index] =
		{
			vert.Position.x,
			vert.Position.y,
			vert.Position.z,
			vert.Normal.x,
			vert.Normal.y,
			vert.Normal.z,
			vert.Color.x,
			vert.Color.y,
			vert.Color.z,
			vert.TextureUV.x,
			vert.TextureUV.y
		};
	}

	for (uint32_t ind : mesh.Indices)
		indSon.push_back(ind);

	json["Vertices"] = vertSon;
	json["Indices"] = indSon;

	contents += json.dump(2);

	return contents;
}

Mesh MeshProvider::Deserialize(const std::string& Contents, bool* SuccessPtr)
{
	*SuccessPtr = true;

	if (Contents.size() == 0)
	{
		MESHPROVIDER_ERROR("Mesh file is empty");
		return Mesh{};
	}

	float version = getVersion(Contents);

	if (version == 0.f)
	{
		MESHPROVIDER_ERROR("No Version header");
		return Mesh{};
	}

	size_t jsonStartLoc = Contents.find("{");
	std::string jsonFileContents = Contents.substr(jsonStartLoc);
	nlohmann::json json = nlohmann::json::parse(jsonFileContents);

	Mesh mesh;

	size_t vertexIndex = 0;

	for (nlohmann::json vertexDesc : json["Vertices"])
	{
		if (vertexDesc.size() % 11 != 0)
		{
			MESHPROVIDER_ERROR("Vertex #" + std::to_string(vertexIndex) + " does not have 11 elements");
			return Mesh{};
		}

		Vertex vertex =
		{
			glm::vec3(vertexDesc[0], vertexDesc[1], vertexDesc[2]),
			glm::vec3(vertexDesc[3], vertexDesc[4], vertexDesc[5]),
			glm::vec3(vertexDesc[6], vertexDesc[7], vertexDesc[8]),
			glm::vec2(vertexDesc[9], vertexDesc[10])
		};

		mesh.Vertices.push_back(vertex);

		vertexIndex += 1;
	}

	for (uint32_t index : json["Indices"])
		mesh.Indices.push_back(index);

	return mesh;
}

void MeshProvider::Save(const Mesh& mesh, const std::string& Path)
{
	std::string contents = this->Serialize(mesh);
	FileRW::WriteFileCreateDirectories(Path, contents, true);
}

void MeshProvider::Save(uint32_t Id, const std::string& Path)
{
	auto meshIt = m_Meshes.find(Id);

	if (meshIt == m_Meshes.end())
		throw("Attempt to save a mesh of invalid ID " + std::to_string(Id) + " to " + Path);
	else
		this->Save(meshIt->second, Path);
}

uint32_t MeshProvider::Assign(const Mesh& mesh, const std::string& InternalName)
{
	static uint32_t CurrentResourceId = 1;
	uint32_t assignedId = CurrentResourceId;

	auto prevPair = m_StringToMeshId.find(InternalName);

	if (prevPair != m_StringToMeshId.end())
	{
		// overwrite the pre-existing mesh
		m_Meshes[prevPair->second] = mesh;
		assignedId = prevPair->second;
	}
	else
	{
		CurrentResourceId += 1;

		m_Meshes.insert(std::pair(assignedId, mesh));
		m_StringToMeshId.insert(std::pair(InternalName, assignedId));
	}

	return assignedId;
}

uint32_t MeshProvider::LoadFromPath(const std::string& Path, bool ShouldLoadAsync)
{
	auto meshIt = m_StringToMeshId.find(Path);

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

			return this->Assign(Mesh{}, "blank");
		}
		else
		{
			Mesh mesh = this->Deserialize(contents, &success);
			if (!success)
				Debug::Log(std::vformat(
					"MeshProvider Failed to load mesh '{}': {}",
					std::make_format_args(Path, s_ErrorString)
				));

			return this->Assign(mesh, Path);
		}
	}
	else
		return meshIt->second;
}

Mesh* MeshProvider::GetMeshResource(uint32_t Id)
{
	auto meshIt = m_Meshes.find(Id);

	if (meshIt == m_Meshes.end())
		throw("Invalid mesh ID " + std::to_string(Id));
	else
		return &meshIt->second;
}

void MeshProvider::FinalizeAsyncLoadedMeshes()
{
}

const std::string& MeshProvider::GetLastErrorString()
{
	return s_ErrorString;
}