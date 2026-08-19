#include <cfloat>
#include <chrono>
#include <nljson.hpp>
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec4.hpp>
#include <tracy/Tracy.hpp>

#include "asset/MeshProvider.hpp"
#include "asset/PrimitiveMeshes.hpp"
#include "asset/Binary.hpp"
#include "render/GpuBuffers.hpp"
#include "ThreadManager.hpp"
#include "render/Renderer.hpp"
#include "Utilities.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

#define MESHPROVIDER_ERROR(err) { *ErrorMessagePtr = err; return {}; }

// is this even correct??
constexpr uint32_t BoneChId = ('B' << 24) | ('O' << 16) | ('N' << 8) | 'E';

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

static Mesh loadMeshVersion2(const std::string_view& FileContents, std::string* ErrorMessagePtr)
{
    size_t binaryStartLoc = FileContents.find_first_of('$');

    if (binaryStartLoc == std::string::npos)
        MESHPROVIDER_ERROR("File did not contain a binary data begin symbol ('$')");

    std::string_view contents{ FileContents.begin() + binaryStartLoc + 1, FileContents.end() };

    if (contents.size() < 12)
        MESHPROVIDER_ERROR("File cannot contain header as binary data is smaller than 12 bytes");

    size_t headerPtr = 0;
    bool fileTooSmallError = false;

    // vertex metadata
    uint32_t vertexMeta = ReadU32(contents, &headerPtr, &fileTooSmallError);
    uint32_t numVerts = ReadU32(contents, &headerPtr, &fileTooSmallError);
    uint32_t numIndices = ReadU32(contents, &headerPtr, &fileTooSmallError);

    if (fileTooSmallError)
        MESHPROVIDER_ERROR("This should have been caught earlier, but header was smaller than 12 bytes");

    bool hasVertexOpacity    = vertexMeta & 0b0'00000001;
    bool hasVertexColor      = vertexMeta & 0b0'00000010;
    bool hasVertexNormal     = vertexMeta & 0b0'00000100;
    bool isRigged            = vertexMeta & 0b0'00001000;
    bool quantizedFloats     = vertexMeta & 0b0'00010000;
    bool quantizedNormals    = vertexMeta & 0b0'00100000;
    bool skinCorrections     = vertexMeta & 0b0'01000000;
    bool isNonNormalized     = vertexMeta & 0b0'10000000;
    bool storedBoneTransform = vertexMeta & 0b1'00000000;

    glm::vec3 assetOrigin = glm::vec3(0.f);
    glm::vec3 assetSize = { 1.f, 1.f, 1.f };

    if (isNonNormalized)
    {
        assetOrigin.x = ReadF32(contents, &headerPtr, &fileTooSmallError);
        assetOrigin.y = ReadF32(contents, &headerPtr, &fileTooSmallError);
        assetOrigin.z = ReadF32(contents, &headerPtr, &fileTooSmallError);

        assetSize.x = ReadF32(contents, &headerPtr, &fileTooSmallError);
        assetSize.y = ReadF32(contents, &headerPtr, &fileTooSmallError);
        assetSize.z = ReadF32(contents, &headerPtr, &fileTooSmallError);
    }

    glm::vec3 uniformVertexNormal = glm::vec3(0.f);
    glm::vec4 uniformVertexRGBA = { 1.f, 1.f, 1.f, 1.f };

    if (!hasVertexNormal)
    {
        float nx = ReadF32(contents, &headerPtr, &fileTooSmallError);
        float ny = ReadF32(contents, &headerPtr, &fileTooSmallError);
        float nz = ReadF32(contents, &headerPtr, &fileTooSmallError);
        uniformVertexNormal = glm::vec3(nx, ny, nz);
    }

    if (!hasVertexColor)
    {
        float r = ReadF32(contents, &headerPtr, &fileTooSmallError);
        float g = ReadF32(contents, &headerPtr, &fileTooSmallError);
        float b = ReadF32(contents, &headerPtr, &fileTooSmallError);

        uniformVertexRGBA = glm::vec4(r, g, b, 1.f);
    }

    if (!hasVertexOpacity)
        uniformVertexRGBA.w = ReadF32(contents, &headerPtr, &fileTooSmallError);

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

    Mesh mesh = {};
    mesh.Vertices.reserve(numVerts);
    mesh.Indices.reserve(numIndices);
    mesh.AssetOrigin = assetOrigin;
    mesh.AssetSize = assetSize;

    size_t cursor = headerPtr;

    for (uint32_t vertexIndex = 0; vertexIndex < numVerts; vertexIndex++)
    {
        float px = ReadF32(contents, &cursor, &fileTooSmallError);
        float py = ReadF32(contents, &cursor, &fileTooSmallError);
        float pz = ReadF32(contents, &cursor, &fileTooSmallError);

        float nx = uniformVertexNormal.x;
        float ny = uniformVertexNormal.y;
        float nz = uniformVertexNormal.z;

        if (hasVertexNormal)
        {
            if (quantizedNormals)
            {
                uint32_t normal = ReadU32(contents, &cursor, &fileTooSmallError);

                // 10 bits per component, not 16!!
                uint16_t x = static_cast<uint16_t>(normal) & 0b0000001111111111;
                uint16_t y = static_cast<uint16_t>(normal >> 10) & 0b0000001111111111;
                uint16_t z = static_cast<uint16_t>(normal >> 20) & 0b0000001111111111;

                nx = x / 512.f - 1.f;
                ny = y / 512.f - 1.f;
                nz = z / 512.f - 1.f;
            }
            else
            {
                nx = ReadF32(contents, &cursor, &fileTooSmallError);
                ny = ReadF32(contents, &cursor, &fileTooSmallError);
                nz = ReadF32(contents, &cursor, &fileTooSmallError);
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
                r = ReadU8(contents, &cursor, &fileTooSmallError) / 255.f;
                g = ReadU8(contents, &cursor, &fileTooSmallError) / 255.f;
                b = ReadU8(contents, &cursor, &fileTooSmallError) / 255.f;
            }
            else
            {
                r = ReadF32(contents, &cursor, &fileTooSmallError);
                g = ReadF32(contents, &cursor, &fileTooSmallError);
                b = ReadF32(contents, &cursor, &fileTooSmallError);
            }
        }

        if (hasVertexOpacity)
        {
            if (quantizedFloats)
                a = ReadU8(contents, &cursor, &fileTooSmallError) / 255.f;
            else
                a = ReadF32(contents, &cursor, &fileTooSmallError);
        }

        float u = 0.f;
        float v = 0.f;

        if (quantizedFloats)
        {
            u = ReadU16(contents, &cursor, &fileTooSmallError) / (float)UINT16_MAX;
            v = ReadU16(contents, &cursor, &fileTooSmallError) / (float)UINT16_MAX;
        }
        else
        {
            u = ReadF32(contents, &cursor, &fileTooSmallError);
            v = ReadF32(contents, &cursor, &fileTooSmallError);
        }

        std::array<uint8_t, 4> bones = { 0, 0, 0, 0 };
        std::array<float, 4> weights = { 0.f, 0.f, 0.f, 0.f };

        if (isRigged)
        {
            uint8_t numJoints = ReadU8(contents, &cursor, &fileTooSmallError);

            if (numJoints > 4)
            {
                Log.WarningF(
                    "Vertex #{} specified {} joints, but only up to 4 are supported, clamping.",
                    vertexIndex, numJoints
                );

                numJoints = 4;
            }

            for (uint8_t i = 0; i < numJoints; i++)
            {
                uint8_t boneId = ReadU8(contents, &cursor, &fileTooSmallError);
                float boneWeight = ReadF32(contents, &cursor, &fileTooSmallError);

                bones[i] = boneId;
                weights[i] = boneWeight;
            }
        }

        mesh.Vertices.emplace_back(
            glm::vec3(px, py, pz),
            glm::vec3(nx, ny, nz),
            glm::vec4(r, g, b, a),
            glm::vec2(u, v),
            bones,
            weights
        );
    }

    for (uint32_t indexIndex = 0; indexIndex < numIndices; indexIndex++)
        mesh.Indices.push_back(ReadU32(contents, &cursor, &fileTooSmallError));

    if (isRigged)
    {
        uint32_t chId = ReadU32(contents, &cursor, &fileTooSmallError);

        if (chId != BoneChId)
            Log.ErrorF(
                "Invalid BONE chunk, expected ID {}, got {}. Skipping",
                BoneChId, chId
            );
        else
        {
            uint8_t numBones = ReadU8(contents, &cursor, &fileTooSmallError);

            if (numBones == 0)
                Log.Warning("Mesh had a BONE chunk and the IsRigged bit, but bone count is 0");

            for (uint8_t boneIdx = 0; boneIdx < numBones; boneIdx++)
            {
                Bone& bone = mesh.Bones.emplace_back();

                uint8_t nameLen = ReadU8(contents, &cursor, &fileTooSmallError);
                bone.Name = std::string(contents, cursor, nameLen);
                cursor += nameLen;

                if (storedBoneTransform)
                {
                    for (int c = 0; c < 4; c++)
                        for (int r = 0; r < 4; r++)
                            bone.Transform[c][r] = ReadF32(contents, &cursor, &fileTooSmallError);
                }

                for (int c = 0; c < 4; c++)
                    for (int r = 0; r < 4; r++)
                        bone.InverseBind[c][r] = ReadF32(contents, &cursor, &fileTooSmallError);

                if (skinCorrections)
                {
                    bone.Parent = ReadU8(contents, &cursor, &fileTooSmallError);
                }
                else
                {
                    bone.Parent = UINT8_MAX;
                    // useless scale vec3
                    cursor += sizeof(float) * 3;
                }

                if (fileTooSmallError)
                {
                    Log.ErrorF(
                        "Reached EoF trying to read Bone ID {}",
                        boneIdx
                    );

                    break;
                }
            }
        }
    }

    if (fileTooSmallError)
    {
        *ErrorMessagePtr = "Binary section of File was too small, and the loader reached the end of it while reading some data";
        // return the mesh because whatever vro
    }

    return mesh;
}

static MeshProvider* s_Instance = nullptr;

void MeshProvider::Initialize(bool InitIsHeadless)
{
    ZoneScoped;
    assert(!s_Instance);

    this->IsHeadless = InitIsHeadless;

    this->Assign(PrimitiveMeshes::Cube(), "!Cube", true); // Cube expected to be at index 1
    this->Assign(PrimitiveMeshes::Quad(), "!Quad", true);
    this->Assign(PrimitiveMeshes::Sphere(), "!Sphere", true);
    this->Assign(PrimitiveMeshes::Cylinder(), "!Cylinder", true);
    this->Assign(PrimitiveMeshes::Cone(), "!Cone", true);
    this->Assign(PrimitiveMeshes::Pyramid(), "!Pyramid", true);

    s_Instance = this;
}

void MeshProvider::Shutdown()
{
    if (IsHeadless)
    {
        s_Instance = nullptr;
        return;
    }

    for (GpuMesh& gpuMesh : m_GpuMeshes)
        gpuMesh.Delete();

    m_Meshes.clear();
    m_StringToMeshId.clear();
    m_LoadingRequests.clear();
    m_GpuMeshes.clear();

    s_Instance = nullptr;
}

MeshProvider::~MeshProvider()
{
    assert(!s_Instance);
}

MeshProvider* MeshProvider::Get()
{
    assert(s_Instance);
    return s_Instance;
}

std::string MeshProvider::Serialize(const Mesh& mesh)
{
    if (mesh.Vertices.size() > UINT32_MAX)
        throw(std::runtime_error("Mesh has too many vertices to serialize"));
    if (mesh.Indices.size() > UINT32_MAX)
        throw(std::runtime_error("Mesh has too many indices to serialize"));

    std::string contents = "PHOENIXF\n#Asset Mesh\n";

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

    // TODO fix
    // corrupts mesh UVs and normals
    bool quantizedFloats = true;
    bool quantizedNormals = false; //true; <-- not working properly

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
    WriteU32(
        contents,
        0b0'00000100 // hasVertexNormal
            + (hasPerVertexAlpha ? 0b0'00000001 : 0)
            + (hasPerVertexColor ? 0b0'00000010 : 0)
            // ruh roh, just realized, the deserialization code
            // already checks the 3rd LSB as per-vertex normal
            // idrc abt actually serializing it tho
            + (isRigged          ? 0b0'00001000 : 0)
            // 18/05/2025 oooooh yeah squeeze out all those bits
            + (quantizedFloats   ? 0b0'00010000 : 0)
            + (quantizedNormals  ? 0b0'00100000 : 0)
            // skinCorrections revision
            + 0b0'01000000
            // stop normalizing meshes to be centered + 1x1x1
            + 0b0'10000000
            // store bone transforms
            + 0b1'00000000
    );

    if (mesh.Vertices.size() > (size_t)UINT32_MAX)
        RAISE_RT("Too many vertices to serialize!");

    if (mesh.Indices.size() > (size_t)UINT32_MAX)
        RAISE_RT("Too many indices to serialize!");

    WriteU32(contents, static_cast<uint32_t>(mesh.Vertices.size()));
    WriteU32(contents, static_cast<uint32_t>(mesh.Indices.size()));

    WriteF32(contents, mesh.AssetOrigin.x);
    WriteF32(contents, mesh.AssetOrigin.y);
    WriteF32(contents, mesh.AssetOrigin.z);

    WriteF32(contents, mesh.AssetSize.x);
    WriteF32(contents, mesh.AssetSize.y);
    WriteF32(contents, mesh.AssetSize.z);

    if (!hasPerVertexColor)
    {
        WriteF32(contents, uniformVertexCol.x);
        WriteF32(contents, uniformVertexCol.y);
        WriteF32(contents, uniformVertexCol.z);
    }

    if (!hasPerVertexAlpha)
        WriteF32(contents, uniformVertexAlpha);

    for (const Vertex& v : mesh.Vertices)
    {
        WriteF32(contents, v.Position.x);
        WriteF32(contents, v.Position.y);
        WriteF32(contents, v.Position.z);

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

            WriteU32(contents, normal);
        }
        else
        {
            WriteF32(contents, v.Normal.x);
            WriteF32(contents, v.Normal.y);
            WriteF32(contents, v.Normal.z);
        }

        if (hasPerVertexColor)
        {
            if (quantizedFloats)
            {
                uint8_t r = static_cast<uint8_t>(v.Paint.x * UINT8_MAX);
                uint8_t g = static_cast<uint8_t>(v.Paint.y * UINT8_MAX);
                uint8_t b = static_cast<uint8_t>(v.Paint.z * UINT8_MAX);

                WriteU8(contents, r);
                WriteU8(contents, g);
                WriteU8(contents, b);
            }
            else
            {
                WriteF32(contents, v.Paint.x);
                WriteF32(contents, v.Paint.y);
                WriteF32(contents, v.Paint.z);
            }
        }

        if (hasPerVertexAlpha)
        {
            if (quantizedFloats)
            {
                uint8_t a = static_cast<uint8_t>(v.Paint.w * UINT8_MAX);
                WriteU8(contents, a);
            }
            else
                WriteF32(contents, v.Paint.w);
        }

        if (quantizedFloats)
        {
            WriteU16(contents, static_cast<uint16_t>(std::clamp(v.TextureUV.x, 0.f, 1.f) * UINT16_MAX));
            WriteU16(contents, static_cast<uint16_t>(std::clamp(v.TextureUV.y, 0.f, 1.f) * UINT16_MAX));
        }
        else
        {
            WriteF32(contents, v.TextureUV.x);
            WriteF32(contents, v.TextureUV.y);
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
            WriteU8(contents, v.InfluencingJoints[0]);
            WriteF32(contents, v.JointWeights[0]);

            WriteU8(contents, v.InfluencingJoints[1]);
            WriteF32(contents, v.JointWeights[1]);

            WriteU8(contents, v.InfluencingJoints[2]);
            WriteF32(contents, v.JointWeights[2]);

            WriteU8(contents, v.InfluencingJoints[3]);
            WriteF32(contents, v.JointWeights[3]);
        }
    }

    for (uint32_t i : mesh.Indices)
        WriteU32(contents, i);

    // why'd i do that... idk
    WriteU32(contents, BoneChId);

    if (mesh.Bones.size() > (size_t)UINT8_MAX)
        RAISE_RT("Too many bones to serialize!");

    uint8_t numBones = static_cast<uint8_t>(mesh.Bones.size());
    contents.push_back(std::bit_cast<char>(numBones));

    for (const Bone& b : mesh.Bones)
    {
        uint8_t nameLen = static_cast<uint8_t>(b.Name.size());
        WriteU8(contents, nameLen);
        contents += b.Name;

        for (int c = 0; c < 4; c++)
            for (int r = 0; r < 4; r++)
                WriteF32(contents, b.Transform[c][r]);

        for (int c = 0; c < 4; c++)
            for (int r = 0; r < 4; r++)
                WriteF32(contents, b.InverseBind[c][r]);

        contents.push_back(std::bit_cast<char>(b.Parent));
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
        MESHPROVIDER_ERROR("Format version 1 is no longer supported");

    if (version >= 2.f && version < 3.f)
        return loadMeshVersion2(Contents, ErrorMessagePtr);

    MESHPROVIDER_ERROR(std::format("Unrecognized mesh version - {}", version));
}

void MeshProvider::Save(const Mesh& mesh, const std::string_view& Path)
{
    ZoneScoped;

    std::string contents = this->Serialize(mesh);
    if (!FileRW::WriteFileCreateDirectories(std::string(Path), contents))
        RAISE_RT("Failed to save mesh to '{}'", Path);
}

void MeshProvider::Save(uint32_t Id, const std::string_view& Path)
{
    this->Save(m_Meshes.at(Id), Path);
}

static void finishAndUploadMesh(Mesh& mesh, MeshProvider::GpuMesh& gpuMesh, bool Headless)
{
    ZoneScoped;

    if (Headless)
        return;

    GpuVertexArray& vao = gpuMesh.VertexArray;
    GpuVertexBuffer& vbo = gpuMesh.VertexBuffer;
    GpuElementBuffer& ebo = gpuMesh.ElementBuffer;

    vao.Bind();

    vao.LinkAttrib(vbo, 0, 3, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, Position));
    vao.LinkAttrib(vbo, 1, 3, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
    vao.LinkAttrib(vbo, 2, 4, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, Paint));
    vao.LinkAttrib(vbo, 3, 2, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, TextureUV));

    vbo.SetBufferData(mesh.Vertices, BufferUsageHint::Static);
    ebo.SetBufferData(mesh.Indices, BufferUsageHint::Static);

    vbo.Unbind();
    ebo.Unbind();

    vao.Bind();

    Renderer* renderer = Renderer::Get();

    assert(renderer->InstancingBuffer != UINT32_MAX);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->InstancingBuffer);

    constexpr int32_t instanceStride = sizeof(Renderer::InstanceDrawInfo);

    // `Transform` matrix
    // 4 vec4's
    glEnableVertexAttribArray(4);
    glEnableVertexAttribArray(5);
    glEnableVertexAttribArray(6);
    glEnableVertexAttribArray(7);
    glVertexAttribDivisor(4, 1);
    glVertexAttribDivisor(5, 1);
    glVertexAttribDivisor(6, 1);
    glVertexAttribDivisor(7, 1);

    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, instanceStride, (void*)offsetof(Renderer::InstanceDrawInfo, TransformRow1));
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, instanceStride, (void*)offsetof(Renderer::InstanceDrawInfo, TransformRow2));
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, instanceStride, (void*)offsetof(Renderer::InstanceDrawInfo, TransformRow3));
    glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, instanceStride, (void*)offsetof(Renderer::InstanceDrawInfo, TransformRow4));

    // vec3s
    // color
    glEnableVertexAttribArray(8);
    glVertexAttribDivisor(8, 1);

    glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, instanceStride, (void*)offsetof(Renderer::InstanceDrawInfo, Color));

    glEnableVertexAttribArray(9);
    glVertexAttribDivisor(9, 1);

    glVertexAttribPointer(9, 1, GL_FLOAT, GL_FALSE, instanceStride, (void*)offsetof(Renderer::InstanceDrawInfo, Transparency));

    if (mesh.Bones.size() > 0)
    {
        gpuMesh.BoneMatrices.reserve(mesh.Bones.size());

        for (const Bone& b : mesh.Bones)
            gpuMesh.BoneMatrices.push_back(b.Transform * b.InverseBind);

        glGenBuffers(1, &gpuMesh.VertexJointDataBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, gpuMesh.VertexJointDataBuffer);
        gpuMesh.VertexArray.Bind();

        constexpr size_t skinnerStride = sizeof(Vertex::InfluencingJoints) + sizeof(Vertex::JointWeights);

        glEnableVertexAttribArray(10);
        glEnableVertexAttribArray(11);
        glVertexAttribDivisor(10, 0);
        glVertexAttribDivisor(11, 0);

        glVertexAttribIPointer(10, 4, GL_UNSIGNED_BYTE, skinnerStride, nullptr);
        glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, skinnerStride, (void*)4);

        std::vector<uint8_t> data;
        data.resize(mesh.Vertices.size() * 4 + mesh.Vertices.size() * sizeof(float) * 4);

        for (uint32_t vi = 0; vi < mesh.Vertices.size(); vi++)
        {
            const Vertex& v = mesh.Vertices[vi];
            memcpy(data.data() + vi * skinnerStride, v.InfluencingJoints.data(), sizeof(uint8_t) * 4);
            memcpy(data.data() + vi * skinnerStride + 4, v.JointWeights.data(), sizeof(float) * 4);
        }

        glBufferData(
            GL_ARRAY_BUFFER,
            data.size(),
            data.data(),
            GL_STATIC_DRAW
        );
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    vao.Unbind();

    gpuMesh.NumIndices = static_cast<uint32_t>(mesh.Indices.size());

    /*
    if (!mesh.MeshDataPreserved && mesh.Bones.size() == 0)
    {
        mesh.Vertices.clear();
        mesh.Indices.clear();
        mesh.Vertices.shrink_to_fit();
        mesh.Indices.shrink_to_fit();
    }
    else
        mesh.MeshDataPreserved = true; // preserve for CPU skinning
    */

    mesh.MeshDataPreserved = true;
    mesh.Vertices.shrink_to_fit();
    mesh.Indices.shrink_to_fit();
}

uint32_t MeshProvider::Assign(Mesh mesh, const std::string& InternalName, bool UploadToGpu)
{
    uint32_t assignedId = static_cast<uint32_t>(m_Meshes.size());

    auto prevPair = m_StringToMeshId.find(InternalName);

    if (prevPair != m_StringToMeshId.end())
    {
        // overwrite the pre-existing mesh
        Mesh preExisting = m_Meshes[prevPair->second];

        m_Meshes[prevPair->second] = mesh;
        assignedId = prevPair->second;

        if (preExisting.GpuId != UINT32_MAX)
        {
            GpuMesh& gpuMesh = m_GpuMeshes.at(preExisting.GpuId);

            if (UploadToGpu)
            {
                finishAndUploadMesh(m_Meshes[prevPair->second], gpuMesh, IsHeadless);
                m_Meshes[prevPair->second].GpuId = preExisting.GpuId;
            }
            else
                gpuMesh.Delete();
        }
    }
    else
    {
        m_StringToMeshId[InternalName] = assignedId;
        m_Meshes.push_back(mesh);

        if (UploadToGpu)
            m_CreateAndUploadGpuMesh(assignedId);
    }

    return assignedId;
}

uint32_t MeshProvider::LoadFromPath(
    const std::string& Path,
    bool ShouldLoadAsync,
    bool PreserveMeshData,
    std::function<void(Mesh&)> PostLoadCallback
)
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

            for (const MeshLoadRequest& loadRequest : m_LoadingRequests)
            {
                if (loadRequest.ResourceId != meshIt->second)
                    continue;

                m_Meshes[meshIt->second].MeshDataPreserved = true;

                loadRequest.Future.wait();
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
        if (ShouldLoadAsync)
        {
            std::promise<Mesh>* promise = new std::promise<Mesh>;

            uint32_t resourceId = this->Assign(Mesh{}, Path);
            m_Meshes.at(resourceId).MeshDataPreserved = PreserveMeshData;

            ThreadManager::Get()->Dispatch(
                "AsyncMeshLoad",
                [promise, this, Path]()
                {
                    bool success = true;
                    std::string contents = FileRW::ReadFile(Path, &success);

                    if (!success)
                    {
                        Log.ErrorF(
                            "Failed to load mesh '{}' asynchronously: File could not be opened",
                            Path
                        );

                        promise->set_value(Mesh{});
                        return;
                    }

                    std::string error;
                    Mesh loadedMesh = this->Deserialize(contents, &error);

                    if (error.size() > 0)
                    {
                        Log.ErrorF(
                            "Failed to load mesh '{}' asynchronously: {}",
                            Path, error
                        );
                    }

                    promise->set_value(loadedMesh);
                },
                false
            );

            m_LoadingRequests.emplace_back(
                promise,
                promise->get_future().share(),
                resourceId,
                PostLoadCallback
            );

            return resourceId;
        }
        else
        {
            ZoneScopedN("LoadSynchronous");

            bool success = true;
            std::string contents = FileRW::ReadFile(Path, &success);

            if (!success)
            {
                Log.ErrorF(
                    "Failed to load mesh '{}' synchronously: File could not be opened",
                    Path
                );

                return this->Assign(Mesh{}, Path);
            }

            std::string error;
            Mesh mesh = this->Deserialize(contents, &error);
            mesh.MeshDataPreserved = PreserveMeshData;

            if (error.size() > 0)
            {
                Log.ErrorF(
                    "Failed to load mesh '{}' synchronously: {}",
                    Path, error
                );
            }
            else
            {
                if (PostLoadCallback)
                    PostLoadCallback(mesh);
            }

            m_CreateAndUploadGpuMesh(mesh);

            return this->Assign(mesh, Path);
        }
    }
    else
    {
        if (PostLoadCallback)
            PostLoadCallback(GetMeshResource(meshIt->second));

        return meshIt->second;
    }
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

    for (auto it = m_LoadingRequests.begin(); it < m_LoadingRequests.end(); it += 1)
    {
        if (!it->Future.valid() || it->Future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            continue;

        ZoneScopedN("MeshReady");

        const Mesh& loadedMesh = it->Future.get();
        Mesh* mesh = &m_Meshes.at(it->ResourceId);

        mesh->Vertices = loadedMesh.Vertices;
        mesh->Indices = loadedMesh.Indices;
        mesh->Bones = loadedMesh.Bones;

        if (it->PostLoadCallback)
            it->PostLoadCallback(*mesh);
        mesh = &m_Meshes.at(it->ResourceId);

        if (!IsHeadless)
            m_CreateAndUploadGpuMesh(*mesh);

        delete it->Promise;

        it = m_LoadingRequests.erase(it);

        if (it >= m_LoadingRequests.end())
            break;
    }
}

void MeshProvider::UnloadMesh(const std::string& Mesh)
{
    if (const auto& it = m_StringToMeshId.find(Mesh); it != m_StringToMeshId.end())
        m_StringToMeshId.erase(it);
}

void MeshProvider::m_CreateAndUploadGpuMesh(Mesh& mesh)
{
    ZoneScoped;

    MeshProvider::GpuMesh& gpuMesh = m_GpuMeshes.emplace_back();

    if (!IsHeadless)
    {
        gpuMesh.VertexArray.Initialize();
        gpuMesh.VertexBuffer.Initialize();
        gpuMesh.ElementBuffer.Initialize();
    }

    finishAndUploadMesh(mesh, gpuMesh, IsHeadless);

    mesh.GpuId = static_cast<uint32_t>(m_GpuMeshes.size() - 1);
}

void MeshProvider::m_CreateAndUploadGpuMesh(uint32_t MeshId)
{
    m_CreateAndUploadGpuMesh(m_Meshes.at(MeshId));
}
