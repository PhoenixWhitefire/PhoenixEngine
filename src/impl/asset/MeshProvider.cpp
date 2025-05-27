#include <iostream>
#include <cfloat>
#include <chrono>
#include <nljson.hpp>
#include <glad/gl.h>
#include <glm/vec4.hpp>
#include <tracy/Tracy.hpp>

#include "asset/MeshProvider.hpp"
#include "asset/PrimitiveMeshes.hpp"
#include "render/GpuBuffers.hpp"
#include "ThreadManager.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

#define MESHPROVIDER_ERROR(err) { *ErrorMessagePtr = err; return {}; }

// is this even correct??
constexpr uint32_t BoneChId = ('B' << 24) | ('O' << 16) | ('N' << 8) | 'E';

static uint16_t readU16(const std::string_view& vec, size_t offset, bool* fileTooSmallPtr)
{
	if (*fileTooSmallPtr || vec.size() - 1 < offset + 1)
	{
		*fileTooSmallPtr = true;
		return UINT16_MAX;
	}

	return *(const uint16_t*)&vec.at(offset);
}

static uint16_t readU16(const std::string_view& vec, size_t* offset, bool* fileTooSmallPtr)
{
	uint16_t u16 = readU16(vec, *offset, fileTooSmallPtr);
	*offset += 2ull;

	return u16;
}

static uint32_t readU32(const std::string_view& vec, size_t offset, bool* fileTooSmallPtr)
{
	if (*fileTooSmallPtr || vec.size() - 1 < offset + 3)
	{
		*fileTooSmallPtr = true;
		return UINT32_MAX;
	}

	uint32_t u32 = *(const uint32_t*)&vec.at(offset);

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
	if (*fileTooSmallPtr || vec.size() - 1 < (*offset) + 3)
	{
		*fileTooSmallPtr = true;
		return FLT_MAX;
	}

	float f32 = *(const float*)&vec.at(*offset);
	*offset += 4ull;

	return f32;
}

static uint8_t readU8(const std::string_view& str, size_t* offset, bool* fileTooSmallPtr)
{
	if (*fileTooSmallPtr || str.size() - 1 < *offset + 1)
	{
		*fileTooSmallPtr = true;
		return UINT8_MAX;
	}

	uint8_t u8 = *(const uint8_t*)&str.at(*offset);
	(*offset)++;

	return u8;
}

static void writeU16(std::string& vec, uint16_t v)
{
	vec.push_back(*(int8_t*)&v);
	vec.push_back(*((int8_t*)&v + 1ull));
}

static void writeU32(std::string& vec, uint32_t v)
{
	vec.push_back(*(int8_t*)&v);
	vec.push_back(*((int8_t*)&v + 1ull));
	vec.push_back(*((int8_t*)&v + 2ull));
	vec.push_back(*((int8_t*)&v + 3ull));

	// TODO why does this generate an out-of-bounds access warning
	//writeU16(vec, v);
	//writeU16(vec, *(uint16_t*)(&v + 2ull));
}

static void writeF32(std::string& vec, float v)
{
	writeU32(vec, *(uint32_t*)&v);
}

static float getVersion(const std::string_view& MapFileContents)
{
	size_t matchLocation = MapFileContents.find("#Version");

	float version = 0.f;

	if (matchLocation != std::string::npos)
	{
		// TODO 06/03/2025 `stof` doesnt accept string_views :(
		std::string subStr = std::string(MapFileContents).substr(matchLocation + 9, 4);
		version = std::stof(subStr);
	}

	return version;
}

static Mesh loadMeshVersion1(const std::string_view& FileContents, std::string* ErrorMessagePtr)
{
	size_t jsonStartLoc = FileContents.find_first_of("{");
	std::string_view jsonFileContents{ FileContents.begin() + jsonStartLoc, FileContents.end() };
	nlohmann::json json = nlohmann::json::parse(jsonFileContents);

	Mesh mesh;
	mesh.Vertices.reserve(json["Vertices"].size());
	mesh.Indices.reserve(json["Indices"].size());

	size_t vertexIndex = 0;

	for (nlohmann::json vertexDesc : json["Vertices"])
	{
		if (vertexDesc.size() % 11 != 0)
			MESHPROVIDER_ERROR("(V1) Vertex #" + std::to_string(vertexIndex) + " does not have 11 elements");

		mesh.Vertices.emplace_back(
			glm::vec3(vertexDesc[0], vertexDesc[1], vertexDesc[2]),
			glm::vec3(vertexDesc[3], vertexDesc[4], vertexDesc[5]),
			glm::vec4(vertexDesc[6], vertexDesc[7], vertexDesc[8], 1.f),
			glm::vec2(vertexDesc[9], vertexDesc[10]),
			std::array<uint8_t, 4>{ UINT8_MAX, UINT8_MAX, UINT8_MAX },
			std::array<float, 4>{ FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX }
		);

		vertexIndex += 1;
	}

	for (uint32_t index : json["Indices"])
		mesh.Indices.push_back(index);

	return mesh;
}

static Mesh loadMeshVersion2(const std::string_view& FileContents, std::string* ErrorMessagePtr)
{
	size_t binaryStartLoc = FileContents.find_first_of('$');

	if (binaryStartLoc == std::string::npos)
		MESHPROVIDER_ERROR("File did not contain a binary data begin symbol ('$')");

	std::string_view contents{ FileContents.begin() + binaryStartLoc + 1, FileContents.end() };

	if (contents.size() < 12)
		MESHPROVIDER_ERROR("File cannot contain header as binary data is smaller than 12 bytes");

	size_t headerPtr{};
	bool fileTooSmallError = false;

	// vertex metadata
	uint32_t vertexMeta = readU32(contents, &headerPtr, &fileTooSmallError);
	uint32_t numVerts = readU32(contents, &headerPtr, &fileTooSmallError);
	uint32_t numIndices = readU32(contents, &headerPtr, &fileTooSmallError);

	if (fileTooSmallError)
		MESHPROVIDER_ERROR("This should have been caught earlier, but header was smaller than 12 bytes");

	bool hasVertexOpacity = vertexMeta & 0b00000001;
	bool hasVertexColor   = vertexMeta & 0b00000010;
	bool hasVertexNormal  = vertexMeta & 0b00000100;
	bool isRigged         = vertexMeta & 0b00001000;
	bool quantizedFloats  = vertexMeta & 0b00010000;
	bool quantizedNormals = vertexMeta & 0b00100000;

	glm::vec3 uniformVertexNormal{};
	glm::vec4 uniformVertexRGBA{ 1.f, 1.f, 1.f, 1.f };

	if (!hasVertexNormal)
		uniformVertexNormal = glm::vec3
		{
			readF32(contents, &headerPtr, &fileTooSmallError),
			readF32(contents, &headerPtr, &fileTooSmallError),
			readF32(contents, &headerPtr, &fileTooSmallError)
		};

	if (!hasVertexColor)
		uniformVertexRGBA = glm::vec4
		{
			readF32(contents, &headerPtr, &fileTooSmallError),
			readF32(contents, &headerPtr, &fileTooSmallError),
			readF32(contents, &headerPtr, &fileTooSmallError),
			1.f
		};

	if (!hasVertexOpacity)
		uniformVertexRGBA.w = readF32(contents, &headerPtr, &fileTooSmallError);

	if (fileTooSmallError)
		MESHPROVIDER_ERROR("File ended during preamble");

	// Px, Py, Pz, (Nx, Ny, Nz), (R, G, B), (A), Tu, Tv, (Bu8, Wf32)
	size_t bytesPerVertex = 12ull
		+ (hasVertexNormal ? (quantizedNormals ? 4 : 12) : 0)
		+ (hasVertexColor ? (quantizedFloats ? 3 : 12) : 0)
		+ (hasVertexOpacity ? (quantizedFloats ? 1 : 4) : 0)
		+ (quantizedFloats ? 4 : 8)
		+ (isRigged ? 5 : 0);

	size_t totalExpectedDataSize = bytesPerVertex * numVerts + numIndices * 4ull;
	size_t actualDataSize = contents.size() - headerPtr;

	if (actualDataSize < totalExpectedDataSize)
		MESHPROVIDER_ERROR(std::format(
			"Binary section of File was expected to be {} bytes, but was {} instead (smaller)",
			totalExpectedDataSize, actualDataSize
		));

	Mesh mesh{};
	mesh.Vertices.reserve(numVerts);
	mesh.Indices.reserve(numIndices);

	size_t cursor = headerPtr;

	for (uint32_t vertexIndex = 0; vertexIndex < numVerts; vertexIndex++)
	{
		float px = readF32(contents, &cursor, &fileTooSmallError);
		float py = readF32(contents, &cursor, &fileTooSmallError);
		float pz = readF32(contents, &cursor, &fileTooSmallError);

		float nx = uniformVertexNormal.x;
		float ny = uniformVertexNormal.y;
		float nz = uniformVertexNormal.z;

		if (hasVertexNormal)
		{
			if (quantizedFloats)
			{
				uint32_t normal = readU32(contents, &cursor, &fileTooSmallError);

				// 10 bytes per component, not 16!!
				uint16_t x = static_cast<uint16_t>(normal) & 0b0000001111111111;
				uint16_t y = static_cast<uint16_t>(normal >> 10) & 0b0000001111111111;
				uint16_t z = static_cast<uint16_t>(normal >> 20) & 0b0000001111111111;

				nx = x / 512.f - 1.f;
				ny = y / 512.f - 1.f;
				nz = z / 512.f - 1.f;
			}
			else
			{
				nx = readF32(contents, &cursor, &fileTooSmallError);
				ny = readF32(contents, &cursor, &fileTooSmallError);
				nz = readF32(contents, &cursor, &fileTooSmallError);
			}
		}

		float r = uniformVertexRGBA.x;
		float g = uniformVertexRGBA.y;
		float b = uniformVertexRGBA.z;
		float a = uniformVertexRGBA.w;

		if (hasVertexColor)
		{
			if (quantizedFloats)
			{
				r = readU8(contents, &cursor, &fileTooSmallError) / 255.f;
				g = readU8(contents, &cursor, &fileTooSmallError) / 255.f;
				b = readU8(contents, &cursor, &fileTooSmallError) / 255.f;
			}
			else
			{
				r = readF32(contents, &cursor, &fileTooSmallError);
				g = readF32(contents, &cursor, &fileTooSmallError);
				b = readF32(contents, &cursor, &fileTooSmallError);
			}
		}

		if (hasVertexOpacity)
		{
			if (quantizedFloats)
				a = readU8(contents, &cursor, &fileTooSmallError) / 255.f;
			else
				a = readF32(contents, &cursor, &fileTooSmallError);
		}

		float u = 0.f;
		float v = 0.f;

		if (quantizedFloats)
		{
			u = readU16(contents, &cursor, &fileTooSmallError) / (float)UINT16_MAX;
			v = readU16(contents, &cursor, &fileTooSmallError) / (float)UINT16_MAX;
		}
		else
		{
			u = readF32(contents, &cursor, &fileTooSmallError);
			v = readF32(contents, &cursor, &fileTooSmallError);
		}

		std::array<uint8_t, 4> bones{ UINT8_MAX, UINT8_MAX, UINT8_MAX, UINT8_MAX };
		std::array<float, 4> weights{ FLT_MAX, FLT_MAX, FLT_MAX };

		if (isRigged)
		{
			uint8_t numJoints = readU8(contents, &cursor, &fileTooSmallError);

			if (numJoints > 4)
			{
				Log::Warning(std::format(
					"Vertex #{} specified {} joints, but only up to 4 are supported, clamping.",
					vertexIndex, numJoints
				));

				numJoints = 4;
			}

			for (uint8_t i = 0; i < numJoints; i++)
			{
				uint8_t boneId = readU8(contents, &cursor, &fileTooSmallError);
				float boneWeight = readF32(contents, &cursor, &fileTooSmallError);

				bones[i] = boneId;
				weights[i] = boneWeight;
			}
		}

		mesh.Vertices.emplace_back(
			glm::vec3{ px, py, pz },
			glm::vec3{ nx, ny, nz },
			glm::vec4{ r, g, b, a },
			glm::vec2{ u, v },
			bones,
			weights
		);
	}

	for (uint32_t indexIndex = 0; indexIndex < numIndices; indexIndex++)
		mesh.Indices.push_back(readU32(contents, &cursor, &fileTooSmallError));

	if (isRigged)
	{
		uint32_t chId = readU32(contents, &cursor, &fileTooSmallError);

		if (chId != BoneChId)
			Log::Error(std::format(
				"Invalid BONE chunk, expected ID {}, got {}. Skipping",
				BoneChId, chId
			));
		else
		{
			uint8_t numBones = readU8(contents, &cursor, &fileTooSmallError);

			if (numBones == 0)
				Log::Warning("Mesh had a BONE chunk and the IsRigged bit, but bone count is 0");

			for (uint8_t boneIdx = 0; boneIdx < numBones; boneIdx++)
			{
				uint8_t nameLen = readU8(contents, &cursor, &fileTooSmallError);
				std::string name = std::string(contents, cursor, nameLen);
				cursor += nameLen;

				glm::mat4 trans =
				{
					readF32(contents, &cursor, &fileTooSmallError),
					readF32(contents, &cursor, &fileTooSmallError),
					readF32(contents, &cursor, &fileTooSmallError),
					readF32(contents, &cursor, &fileTooSmallError),

					readF32(contents, &cursor, &fileTooSmallError),
					readF32(contents, &cursor, &fileTooSmallError),
					readF32(contents, &cursor, &fileTooSmallError),
					readF32(contents, &cursor, &fileTooSmallError),

					readF32(contents, &cursor, &fileTooSmallError),
					readF32(contents, &cursor, &fileTooSmallError),
					readF32(contents, &cursor, &fileTooSmallError),
					readF32(contents, &cursor, &fileTooSmallError),

					readF32(contents, &cursor, &fileTooSmallError),
					readF32(contents, &cursor, &fileTooSmallError),
					readF32(contents, &cursor, &fileTooSmallError),
					readF32(contents, &cursor, &fileTooSmallError)
				};

				glm::vec3 scale =
				{
					readF32(contents, &cursor, &fileTooSmallError),
					readF32(contents, &cursor, &fileTooSmallError),
					readF32(contents, &cursor, &fileTooSmallError)
				};

				if (fileTooSmallError)
				{
					Log::Error(std::format(
						"Reached EoF trying to read Bone ID {}",
						boneIdx
					));

					break;
				}

				mesh.Bones.emplace_back(name, trans, scale);
			}
		}
	}

	if (fileTooSmallError)
	{
		*ErrorMessagePtr = "Binary section of File was too small, and the loader reached the end of it while reading some data";
		// return the mesh because whatever bro
	}

	return mesh;
}

static MeshProvider* s_Instance = nullptr;

void MeshProvider::Initialize()
{
	ZoneScoped;
	assert(!s_Instance);

	this->Assign(PrimitiveMeshes::Cube(), "!Cube", true);
	this->Assign(PrimitiveMeshes::Quad(), "!Quad", true);

	s_Instance = this;
}

void MeshProvider::Shutdown()
{
	for (GpuMesh& gpuMesh : m_GpuMeshes)
		gpuMesh.Delete();

	m_Meshes.clear();
	m_StringToMeshId.clear();
	m_MeshFutures.clear();
	m_MeshPromises.clear();
	m_MeshPromiseResourceIds.clear();
	m_GpuMeshes.clear();

	s_Instance = nullptr;
}

MeshProvider::~MeshProvider()
{
	assert(!s_Instance);
}

MeshProvider* MeshProvider::Get()
{
	return s_Instance;
}

std::string MeshProvider::Serialize(const Mesh& mesh)
{
	if (mesh.Vertices.size() > UINT32_MAX)
		throw("Mesh has too many vertices to serialize");
	if (mesh.Indices.size() > UINT32_MAX)
		throw("Mesh has too many indices to serialize");

	std::string contents = "PHNXENGI\n#Asset Mesh\n";

	bool hasPerVertexColor = false;
	bool hasPerVertexAlpha = false;
	bool isRigged = !mesh.Bones.empty();

	contents += "#Version 2.20\n";

	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	std::chrono::year_month_day ymd = std::chrono::floor<std::chrono::days>(now);

	contents += "#Date "
		+ std::to_string((uint32_t)ymd.day()) + "-"
		+ std::to_string((uint32_t)ymd.month()) + "-"
		+ std::to_string((int32_t)ymd.year()) + "\n\n"
		+ "$";

	glm::vec3 uniformVertexCol = glm::vec3(mesh.Vertices.at(0).Paint);
	float uniformVertexAlpha = mesh.Vertices.at(0).Paint.w;

	// two separate loops in case one early-exits
	for (const Vertex& v : mesh.Vertices)
		if (glm::vec3(v.Paint) != uniformVertexCol)
		{
			hasPerVertexColor = true;
			break;
		}

	for (const Vertex& v : mesh.Vertices)
		if (v.Paint.w != uniformVertexAlpha)
		{
			hasPerVertexAlpha = true;
			break;
		}

	bool quantizedFloats = true;
	bool quantizedNormals = true;

	for (const Vertex& v : mesh.Vertices)
	{
		if (hasPerVertexColor)
			if (v.Paint.r < 0.f || v.Paint.r > 1.f
				|| v.Paint.g < 0.f || v.Paint.g > 1.f
				|| v.Paint.b < 0.f || v.Paint.b > 1.f
			)
			{
				quantizedFloats = false;
				break;
			}
		
		if (hasPerVertexAlpha)
			if (v.Paint.w < 0.f || v.Paint.w > 1.f)
			{
				quantizedFloats = false;
				break;
			}
		
		if (v.TextureUV.x < -0.01f || v.TextureUV.x > 1.01f
			|| v.TextureUV.y < -0.01f || v.TextureUV.y > 1.01f
		)
		{
			quantizedFloats = false;
			break;
		}
	}

	for (const Vertex& v : mesh.Vertices)
		if (v.Normal.x < -1.f || v.Normal.x > 1.f
			|| v.Normal.y < -1.f || v.Normal.y > 1.f
			|| v.Normal.z < -1.f || v.Normal.z > 1.f
		)
		{
			quantizedNormals = false;
			break;
		}
	
	// minimum: Px, Py, Pz, Nx, Ny, Nz, Tu, Tv
	// Optional: (R, G, B), (A)
	size_t floatsPerVertex = 8ull + (hasPerVertexColor * 3ull) + (hasPerVertexAlpha);

	// 12b header + 4b padding
	contents.reserve(16ull + mesh.Vertices.size() * (floatsPerVertex * 4ull) + mesh.Indices.size() * 4ull + contents.size());

	// per-vertex normal, color and opacity flags
	// 02/01/2025: Rigged flag added
	// ... also forgot to implement the Normal flag
	// not going to make a difference unless you have a
	// flat quad anyway
	// 15/05/2025 quantizedFloats flag added
	// also just realizing that this header isnt actually 1 byte...
	// god i am stupid. i always thought in my head this was 1 byte i do not remember
	// making it 4 bytes as being intentional
	writeU32(
		contents,
		0b00000100
			+ (hasPerVertexAlpha ? 0b00000001 : 0)
			+ (hasPerVertexColor ? 0b00000010 : 0)
			// ruh roh, just realized, the deserialization code
			// already checks the 3rd LSB as per-vertex normal
			// idrc abt actually serializing it tho
			+ (isRigged          ? 0b00001000 : 0)
			// 18/05/2025 oooooh yeah squeeze out all those bits
			+ (quantizedFloats   ? 0b00010000 : 0)
			+ (quantizedNormals  ? 0b00100000 : 0)
	);
	writeU32(contents, static_cast<uint32_t>(mesh.Vertices.size()));
	writeU32(contents, static_cast<uint32_t>(mesh.Indices.size()));

	if (!hasPerVertexColor)
	{
		writeF32(contents, uniformVertexCol.x);
		writeF32(contents, uniformVertexCol.y);
		writeF32(contents, uniformVertexCol.z);
	}

	if (!hasPerVertexAlpha)
		writeF32(contents, uniformVertexAlpha);

	for (const Vertex& v : mesh.Vertices)
	{
		writeF32(contents, v.Position.x);
		writeF32(contents, v.Position.y);
		writeF32(contents, v.Position.z);

		if (quantizedNormals)
		{
			uint32_t normal = 0;

			// 10 bits per component, so x512
			// (2^10 = 1024, half in the negative, half in the positive)
			uint16_t x = static_cast<uint16_t>(std::clamp(v.Normal.x + 1.f, 0.f, 2.f) * 512);
			uint16_t y = static_cast<uint16_t>(std::clamp(v.Normal.y + 1.f, 0.f, 2.f) * 512);
			uint16_t z = static_cast<uint16_t>(std::clamp(v.Normal.z + 1.f, 0.f, 2.f) * 512);

			normal += x;
			normal += y << 10;
			normal += z << 20;

			writeU32(contents, normal);
		}
		else
		{
			writeF32(contents, v.Normal.x);
			writeF32(contents, v.Normal.y);
			writeF32(contents, v.Normal.z);
		}

		if (hasPerVertexColor)
		{
			if (quantizedFloats)
			{
				uint8_t r = static_cast<uint8_t>(v.Paint.x * UINT8_MAX);
				uint8_t g = static_cast<uint8_t>(v.Paint.y * UINT8_MAX);
				uint8_t b = static_cast<uint8_t>(v.Paint.z * UINT8_MAX);

				contents.push_back(*(int8_t*)&r);
				contents.push_back(*(int8_t*)&g);
				contents.push_back(*(int8_t*)&b);
			}
			else
			{
				writeF32(contents, v.Paint.x);
				writeF32(contents, v.Paint.y);
				writeF32(contents, v.Paint.z);
			}
		}

		if (hasPerVertexAlpha)
		{
			if (quantizedFloats)
			{
				uint8_t a = static_cast<uint8_t>(v.Paint.w * UINT8_MAX);
				contents.push_back(*(int8_t*)&a);
			}
			else
				writeF32(contents, v.Paint.w);
		}

		if (quantizedFloats)
		{
			writeU16(contents, static_cast<uint16_t>(v.TextureUV.x * UINT16_MAX));
			writeU16(contents, static_cast<uint16_t>(v.TextureUV.y * UINT16_MAX));
		}
		else
		{
			writeF32(contents, v.TextureUV.x);
			writeF32(contents, v.TextureUV.y);
		}

		if (isRigged)
		{
			// number of bone slots specified
			// max 4 rn 02/01/2025
			// invalid bones (UINT8_MAX i.e. 255) are still specified
			// to reduce serialization/deserialization complexity
			contents.push_back(4);

			// bone id, then weight
			// felt easier
			contents.push_back(*(const int8_t*)&v.InfluencingJoints[0]);
			writeF32(contents, v.JointWeights[0]);

			contents.push_back(*(const int8_t*)&v.InfluencingJoints[1]);
			writeF32(contents, v.JointWeights[1]);

			contents.push_back(*(const int8_t*)&v.InfluencingJoints[2]);
			writeF32(contents, v.JointWeights[2]);

			contents.push_back(*(const int8_t*)&v.InfluencingJoints[3]);
			writeF32(contents, v.JointWeights[3]);
		}
	}

	for (uint32_t i : mesh.Indices)
		writeU32(contents, i);

	// why'd i do that... idk
	writeU32(contents, BoneChId);

	uint8_t numBones = static_cast<uint8_t>(mesh.Bones.size());
	contents.push_back(*(const int8_t*)&numBones);

	for (const Bone& b : mesh.Bones)
	{
		uint8_t nameLen = static_cast<uint8_t>(b.Name.size());
		contents.push_back(*(const int8_t*)&nameLen);
		contents += b.Name;

		// 4x4 transform matrix
		writeF32(contents, b.Transform[0][0]);
		writeF32(contents, b.Transform[0][1]);
		writeF32(contents, b.Transform[0][2]);
		writeF32(contents, b.Transform[0][3]);

		writeF32(contents, b.Transform[1][0]);
		writeF32(contents, b.Transform[1][1]);
		writeF32(contents, b.Transform[1][2]);
		writeF32(contents, b.Transform[1][3]);

		writeF32(contents, b.Transform[2][0]);
		writeF32(contents, b.Transform[2][1]);
		writeF32(contents, b.Transform[2][2]);
		writeF32(contents, b.Transform[2][3]);

		writeF32(contents, b.Transform[3][0]);
		writeF32(contents, b.Transform[3][1]);
		writeF32(contents, b.Transform[3][2]);
		writeF32(contents, b.Transform[3][3]);

		// vec3 scale
		writeF32(contents, b.Scale.x);
		writeF32(contents, b.Scale.y);
		writeF32(contents, b.Scale.z);
	}

	return contents;
}

Mesh MeshProvider::Deserialize(const std::string_view& Contents, std::string* ErrorMessagePtr)
{
	if (Contents.empty())
		MESHPROVIDER_ERROR("Mesh file is empty");

	float version = getVersion(Contents);

	if (version == 0.f)
		MESHPROVIDER_ERROR("No Version header");

	if (version >= 1.f && version < 2.f)
		return loadMeshVersion1(Contents, ErrorMessagePtr);

	if (version >= 2.f && version < 3.f)
		return loadMeshVersion2(Contents, ErrorMessagePtr);

	MESHPROVIDER_ERROR(std::string("Unrecognized mesh version - ") + std::to_string(version));
}

void MeshProvider::Save(const Mesh& mesh, const std::string_view& Path)
{
	ZoneScoped;

	std::string contents = this->Serialize(mesh);
	FileRW::WriteFileCreateDirectories(Path, contents, true);
}

void MeshProvider::Save(uint32_t Id, const std::string_view& Path)
{
	this->Save(m_Meshes.at(Id), Path);
}

static size_t getMeshSizeCpu(const Mesh& mesh)
{
	size_t size = 0;

	if (!mesh.MeshDataPreserved)
	{
		size += mesh.Bones.capacity() * sizeof(Bone);
		size += sizeof(mesh);

		for (const Bone& b : mesh.Bones)
			size += b.Name.capacity();
	}
	else
		size += mesh.Vertices.capacity() * sizeof(Vertex) + mesh.Indices.capacity() * sizeof(uint32_t);

	return size;
}

static void uploadMeshDataToGpuMesh(Mesh& mesh, MeshProvider::GpuMesh& gpuMesh)
{
	ZoneScoped;

	GpuVertexArray& vao = gpuMesh.VertexArray;
	GpuVertexBuffer& vbo = gpuMesh.VertexBuffer;
	GpuElementBuffer& ebo = gpuMesh.ElementBuffer;

	vao.Bind();

	vao.LinkAttrib(vbo, 0, 3, GL_FLOAT, sizeof(Vertex), (void*)0);
	vao.LinkAttrib(vbo, 1, 3, GL_FLOAT, sizeof(Vertex), (void*)(3 * sizeof(float)));
	vao.LinkAttrib(vbo, 2, 4, GL_FLOAT, sizeof(Vertex), (void*)(6 * sizeof(float)));
	vao.LinkAttrib(vbo, 3, 2, GL_FLOAT, sizeof(Vertex), (void*)(10 * sizeof(float)));
	vao.LinkAttrib(vbo, 4, 4, GL_FLOAT, sizeof(Vertex), (void*)(12 * sizeof(float)));
	vao.LinkAttrib(vbo, 5, 4, GL_FLOAT, sizeof(Vertex), (void*)(16 * sizeof(float)));

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
		mesh.Vertices.shrink_to_fit();
		mesh.Indices.shrink_to_fit();
	}
}

uint32_t MeshProvider::Assign(Mesh mesh, const std::string_view& InternalName, bool UploadToGpu)
{
	uint32_t assignedId = static_cast<uint32_t>(m_Meshes.size());

	auto prevPair = m_StringToMeshId.find(std::string(InternalName));

	// 24/01/2025 `Memory` namespace can't handle
	// placement new rn, which is why i assume the counter is
	// always 0
	// use the tag MEMPLACEMENTNEW to easily grep where i do this hack
	// when this can be avoided in the future :)
	Memory::Counters[(size_t)Memory::Category::Mesh] += getMeshSizeCpu(mesh);

	if (prevPair != m_StringToMeshId.end())
	{
		// overwrite the pre-existing mesh
		Mesh preExisting = m_Meshes[prevPair->second];
		Memory::Counters[(size_t)Memory::Category::Mesh] -= getMeshSizeCpu(preExisting);

		m_Meshes[prevPair->second] = mesh;
		assignedId = prevPair->second;

		if (preExisting.GpuId != UINT32_MAX)
		{
			GpuMesh& gpuMesh = m_GpuMeshes.at(preExisting.GpuId);

			if (UploadToGpu)
			{
				uploadMeshDataToGpuMesh(m_Meshes[prevPair->second], gpuMesh);
				m_Meshes[prevPair->second].GpuId = preExisting.GpuId;
			}
			else
				gpuMesh.Delete();
		}
	}
	else
	{
		m_StringToMeshId.insert(std::pair(InternalName, assignedId));
		m_Meshes.push_back(mesh);

		if (UploadToGpu)
			m_CreateAndUploadGpuMesh(assignedId);
	}

	return assignedId;
}

uint32_t MeshProvider::LoadFromPath(const std::string_view& Path, bool ShouldLoadAsync, bool PreserveMeshData)
{
	ZoneScoped;
	ZoneTextF("%s", Path.data());

	auto meshIt = m_StringToMeshId.find(std::string(Path));

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
			Log::Error(std::format(
				"MeshProvider Failed to load mesh '{}': Invalid path/File could not be opened",
				Path
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

				// otherwise we get use-after-free if the source of
				// Path is de-alloc'd
				std::string pathCapturable{ Path };

				ThreadManager::Get()->Dispatch(
					[promise, resourceId, this, pathCapturable, contents]()
					{
						ZoneScopedN("AsyncMeshLoad");

						std::string error;
						Mesh loadedMesh = this->Deserialize(contents, &error);

						if (error.size() > 0)
							Log::Error(std::format(
								"MeshProvider failed to load mesh '{}' asynchronously: {}",
								pathCapturable, error
							));

						promise->set_value(loadedMesh);
					},
					false
				);

				m_MeshPromises.push_back(promise);
				m_MeshFutures.push_back(promise->get_future().share());
				m_MeshPromiseResourceIds.push_back(resourceId);

				return resourceId;
			}
			else
			{
				ZoneScopedN("LoadSynchronous");

				std::string error;
				Mesh mesh = this->Deserialize(contents, &error);
				mesh.MeshDataPreserved = PreserveMeshData;

				if (error.size() > 0)
					Log::Error(std::format(
						"MeshProvider failed to load mesh '{}': {}",
						Path, error
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
	ZoneScoped;

	size_t numMeshPromises = m_MeshPromises.size();
	size_t numMeshFutures = m_MeshFutures.size();
	size_t numMeshResourceIds = m_MeshPromiseResourceIds.size();

	if (numMeshPromises != numMeshFutures || numMeshFutures != numMeshResourceIds)
	{
		Log::Error(std::format(
			"FinalizeAsyncLoadedMeshes had {} promises, {} futures and {} resource IDs, cannot proceed safely",
			numMeshPromises, numMeshFutures, numMeshResourceIds
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
		mesh.Bones = loadedMesh.Bones;

		m_CreateAndUploadGpuMesh(mesh);

		m_MeshPromises.erase(m_MeshPromises.begin() + promiseIndex);
		m_MeshFutures.erase(m_MeshFutures.begin() + promiseIndex);
		m_MeshPromiseResourceIds.erase(m_MeshPromiseResourceIds.begin() + promiseIndex);

		delete promise;
	}
}

void MeshProvider::m_CreateAndUploadGpuMesh(Mesh& mesh)
{
	ZoneScoped;

	m_GpuMeshes.emplace_back();

	MeshProvider::GpuMesh& gpuMesh = m_GpuMeshes.back();

	gpuMesh.VertexArray.Initialize();
	gpuMesh.VertexBuffer.Initialize();
	gpuMesh.ElementBuffer.Initialize();

	uploadMeshDataToGpuMesh(mesh, gpuMesh);

	mesh.GpuId = static_cast<uint32_t>(m_GpuMeshes.size() - 1);
}

void MeshProvider::m_CreateAndUploadGpuMesh(uint32_t MeshId)
{
	m_CreateAndUploadGpuMesh(m_Meshes.at(MeshId));
}
