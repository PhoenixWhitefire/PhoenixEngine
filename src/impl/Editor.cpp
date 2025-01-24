#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <ImGuiFD/ImGuiFD.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_internal.h>
#include <tracy/Tracy.hpp>
#include <glad/gl.h>
#include <fstream>
#include <set>

#include "Editor.hpp"

#include "asset/MaterialManager.hpp"
#include "asset/TextureManager.hpp"
#include "asset/MeshProvider.hpp"

#include "gameobject/Camera.hpp"
#include "gameobject/Script.hpp"

#include "Utilities.hpp"
#include "UserInput.hpp"
#include "Memory.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

constexpr uint32_t OBJECT_NEW_CLASSNAME_BUFSIZE = 16;
constexpr uint32_t MATERIAL_NEW_NAME_BUFSIZE = 64;
constexpr uint32_t MATERIAL_TEXTUREPATH_BUFSIZE = 64;
constexpr const char* MATERIAL_NEW_NAME_DEFAULT = "newmaterial";

static const char* ParentString = "[Parent]";

static bool TextEditorEnabled = false;
static std::string TextEditorFile = "<NOT_SELECTED>";

static nlohmann::json DefaultNewMaterial = 
{
	{ "ColorMap", "textures/materials/plastic.png" },
	{ "specExponent", 32.f },
	{ "specMultiply", 0.5f }
};

static std::unordered_map<std::string, std::string> ClassIcons{};

static GpuFrameBuffer MtlEditorPreview;
static Scene MtlPreviewScene =
{
	// cube
	{
		RenderItem
		{
			0u,
			glm::mat4(1.f),
			glm::vec3(1.f, 1.f, 1.f),
			0u,
			Color(1.f, 1.f, 1.f),
			0.f,
			0.f,
			0.f,
			FaceCullingMode::BackFace
		}
	},
	// light source
	{
		{
			LightType::Directional,
			false,
			Vector3(0.57f, 0.57f, 0.57f),
			Color(1.f, 1.f, 1.f)
		}
	},
	{} // used shaders
};
static Renderer* MtlPreviewRenderer = nullptr;
static Object_Camera* MtlPreviewCamera = nullptr;
static glm::mat4 MtlPreviewCamOffset = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -2.f));
static glm::mat4 MtlPreviewCamDefaultRotation = glm::eulerAngleYXZ(glm::radians(168.f), glm::radians(12.f), 0.f);

static std::string ErrorTooltipMessage = "No Error Dummy";
static double ErrorTooltipTimeRemaining = 0.f;

Editor::Editor(Renderer* renderer)
{
	m_MtlCreateNameBuf = BufferInitialize(MATERIAL_NEW_NAME_BUFSIZE, MATERIAL_NEW_NAME_DEFAULT);
	m_MtlLoadNameBuf = BufferInitialize(MATERIAL_NEW_NAME_BUFSIZE, MATERIAL_NEW_NAME_DEFAULT);
	m_MtlDiffuseBuf = BufferInitialize(MATERIAL_TEXTUREPATH_BUFSIZE);
	m_MtlSpecBuf = BufferInitialize(MATERIAL_TEXTUREPATH_BUFSIZE);
	m_MtlNormalBuf = BufferInitialize(MATERIAL_TEXTUREPATH_BUFSIZE);
	m_MtlEmissionBuf = BufferInitialize(MATERIAL_TEXTUREPATH_BUFSIZE);
	m_MtlShpBuf = BufferInitialize(MATERIAL_TEXTUREPATH_BUFSIZE);

	MtlEditorPreview.Initialize(256, 256);
	MtlPreviewRenderer = renderer;
	MtlPreviewCamera = static_cast<Object_Camera*>(GameObject::Create("Camera"));
	MtlPreviewCamera->Transform = MtlPreviewCamDefaultRotation * MtlPreviewCamOffset;
	MtlPreviewCamera->FieldOfView = 50.f;

	nlohmann::json iconsJson = nlohmann::json::parse(FileRW::ReadFile("textures/editor-icons/icons.json"));

	for (auto it = iconsJson.begin(); it != iconsJson.end(); ++it)
		ClassIcons[it.key()] = (std::string)it.value();
}

void Editor::Update(double DeltaTime)
{
	ErrorTooltipTimeRemaining -= DeltaTime;
}

static bool mtlIterator(void*, int index, const char** outText)
{
	MaterialManager* mtlManager = MaterialManager::Get();

	RenderMaterial& selected = mtlManager->GetLoadedMaterials()[index];

	*outText = selected.Name.c_str();

	return true;
}

static bool mtlUniformIterator(void* array, int index, const char** outText)
{
	*outText = ((std::vector<std::string>*)array)->at(index).c_str();

	return true;
}

static std::string getFileDirectory(const std::string& FilePath)
{
	size_t lastFwdSlash = FilePath.find_last_of("/");

	if (lastFwdSlash == std::string::npos)
		return "resources/";
	else
	{
		std::string fileDir = FilePath.substr(0, lastFwdSlash);

		if (fileDir.find("resources/") == std::string::npos)
			fileDir = "resources/" + fileDir;

		return fileDir;
	}
}

static char* TextEditorEntryBuffer = nullptr;
static size_t TextEditorEntryBufferCapacity = 0;
static std::fstream* TextEditorFileStream = nullptr;
static std::set<std::string> TextEditorQuickSelectFiles;

static void textEditorSaveFile()
{
	if (!TextEditorEntryBuffer)
	{
		ErrorTooltipMessage = "Text Editor tried to save file, but had no text buffer";
		ErrorTooltipTimeRemaining = 3.f;
		return;
	}

	std::string contents = TextEditorEntryBuffer;

	// always flush the buffer. if a new file is created (i.e. "<NOT_SELECTED>"), and the
	// `contents.empty` early-out triggers as the user opens another file, the new file
	// will be overwritten with the empty contents
	// 12/02/2024
	Memory::Free(TextEditorEntryBuffer);
	TextEditorEntryBuffer = nullptr;

	if (TextEditorFile == "" || TextEditorFile == "<NOT_SELECTED>")
	{
		if (contents.empty())
			return;

		static uint32_t ErrCount = 0;
		ErrCount++;

		TextEditorFile = "texteditor_default_" + std::to_string(ErrCount) + ".txt";

		ErrorTooltipMessage = "Text Editor tried to save a file with no path. Will be saved to " + TextEditorFile;
		ErrorTooltipTimeRemaining = 5.f;
	}

	if (TextEditorFileStream && TextEditorFileStream->is_open())
	{
		TextEditorFileStream->close();
		delete TextEditorFileStream;
		TextEditorFileStream = nullptr;
	}

	if (TextEditorFile.find("scripts/") != std::string::npos
		&& TextEditorFile.find("resources/scripts/") == std::string::npos
	)
		TextEditorFile = "resources/" + TextEditorFile;

	size_t lastPeriod = TextEditorFile.find_last_of(".");
	size_t lastFwdSlash = TextEditorFile.find_last_of("/");

	if (lastPeriod == std::string::npos || (lastFwdSlash != std::string::npos && lastFwdSlash > lastPeriod))
	{
		TextEditorFile += ".txt";
		ErrorTooltipMessage = "File will be saved as " + TextEditorFile;
		ErrorTooltipTimeRemaining = 2.f;
	}

	FileRW::WriteFile(TextEditorFile, contents, true);
}

static void invokeTextEditor(const std::string& File)
{
	if (TextEditorEntryBuffer)
		textEditorSaveFile();

	TextEditorEnabled = true;
	TextEditorFile = File;
	TextEditorQuickSelectFiles.insert(File);
}

static void renderTextEditor()
{
	ZoneScopedC(tracy::Color::DarkSeaGreen);

	if (!TextEditorEnabled)
	{
		if (TextEditorEntryBuffer)
			textEditorSaveFile();
		TextEditorFile = "<NOT_SELECTED>";

		return;
	}

	ImGui::Begin("Text Editor", 0, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar);

	ImGui::BeginMenuBar();

	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::MenuItem("New"))
		{
			textEditorSaveFile();
			TextEditorFile = "<NOT_SELECTED>";

			TextEditorEntryBuffer = BufferInitialize(512);
			TextEditorEntryBufferCapacity = 512;
		}

		if (ImGui::MenuItem("Open"))
		{
			TextEditorQuickSelectFiles.insert(TextEditorFile);

			textEditorSaveFile();

			std::string curDir = getFileDirectory(TextEditorFile);
			ImGuiFD::OpenDialog("Text Editor Open File", ImGuiFDMode_LoadFile, curDir.c_str());
		}

		if (ImGui::MenuItem("Save", NULL, nullptr, TextEditorFile != "" && TextEditorFile != "<NOT_SELECTED>"))
			textEditorSaveFile();

		if (ImGui::MenuItem("Save As"))
		{
			std::string curDir = getFileDirectory(TextEditorFile);
			ImGuiFD::OpenDialog(
				"Text Editor Save File As",
				ImGuiFDMode_LoadFile,
				curDir.c_str()
			);
		}

		if (ImGui::MenuItem("Close"))
		{
			TextEditorEnabled = false;
			textEditorSaveFile();
		}

		ImGui::EndMenu();
	}

	std::string selectorMenuText = "Active: " + TextEditorFile;
	bool quickSelectOpen = ImGui::BeginMenu(selectorMenuText.c_str());
	ImGui::SetItemTooltip("Quick Select from recent files");

	if (quickSelectOpen)
	{
		std::set<std::string>::reverse_iterator rit;

		for (rit = TextEditorQuickSelectFiles.rbegin(); rit != TextEditorQuickSelectFiles.rend(); rit++)
			if (ImGui::MenuItem(rit->data()))
			{
				textEditorSaveFile();
				TextEditorFile = rit->data();
			}

		ImGui::EndMenu();
	}

	ImGui::EndMenuBar();

	if (ImGuiFD::BeginDialog("Text Editor Open File"))
	{
		if (ImGuiFD::ActionDone())
		{
			if (ImGuiFD::SelectionMade())
			{
				textEditorSaveFile();
				TextEditorFile = ImGuiFD::GetSelectionPathString(0);

				if (!TextEditorFile.find("resources/"))
					TextEditorFile.insert(0, "./"); // for `FileRW::GetAbsolutePath`

				TextEditorQuickSelectFiles.insert(TextEditorFile);
			}

			ImGuiFD::CloseCurrentDialog();
		}

		ImGuiFD::EndDialog();
	}

	if (ImGuiFD::BeginDialog("Text Editor Save File As"))
	{
		if (ImGuiFD::ActionDone())
		{
			if (ImGuiFD::SelectionMade())
			{
				TextEditorFile = ImGuiFD::GetSelectionPathString(0);
				if (!TextEditorFile.find("resources/"))
					TextEditorFile.insert(0, "./"); // for `FileRW::GetAbsolutePath`

				textEditorSaveFile();

				TextEditorQuickSelectFiles.insert(TextEditorFile);
			}

			ImGuiFD::CloseCurrentDialog();
		}

		ImGuiFD::EndDialog();
	}

	if (!TextEditorEntryBuffer)
	{
		if (TextEditorFileStream)
		{
			TextEditorFileStream->close();
			delete TextEditorFileStream;
		}

		TextEditorFileStream = new std::fstream(FileRW::GetAbsolutePath(TextEditorFile));
		
		std::string scriptContents = "";

		if (!(*TextEditorFileStream) || !TextEditorFileStream->is_open())
		{
			ErrorTooltipMessage = "File couldn't be opened";
			ErrorTooltipTimeRemaining = 5.f;
			TextEditorFile = "<NOT_SELECTED>";

			TextEditorEntryBuffer = BufferInitialize(512);
			TextEditorEntryBufferCapacity = 512;
		}
		else
		{
			TextEditorFileStream->seekg(0, std::ios::end);

			scriptContents.resize(TextEditorFileStream->tellg());
			TextEditorFileStream->seekg(0, std::ios::beg);

			TextEditorFileStream->read(&scriptContents[0], scriptContents.size());

			TextEditorEntryBufferCapacity = scriptContents.size() + 256;
			TextEditorEntryBuffer = (char*)Memory::Alloc(TextEditorEntryBufferCapacity);

			CopyStringToBuffer(TextEditorEntryBuffer, TextEditorEntryBufferCapacity, scriptContents);
		}
	}
	
	if (TextEditorEntryBuffer)
		ImGui::InputTextMultiline(
			"##",
			TextEditorEntryBuffer,
			TextEditorEntryBufferCapacity,
			ImGui::GetContentRegionAvail()
		);

	ImGui::End();
}

static void uniformsEditor(std::unordered_map<std::string, Reflection::GenericValue>& Uniforms, int* Selection)
{
	ZoneScopedC(tracy::Color::DarkSeaGreen);

	if ((*Selection) + 1ull > Uniforms.size())
		*Selection = -1;

	static int TypeId = 0;

	static std::string NewUniformNameBuf;
	NewUniformNameBuf.resize(32);

	ImGui::InputText("Variable Name", &NewUniformNameBuf);
	ImGui::InputInt("Variable Type", &TypeId);
	ImGui::SetItemTooltip("0=Bool, 1=Int, 2=Float");

	TypeId = std::clamp(TypeId, 0, 2);

	if (ImGui::Button("Add"))
	{
		Reflection::GenericValue initialValue;

		switch (TypeId)
		{
		case (0):
		{
			initialValue = true;
			break;
		}
		case (1):
		{
			initialValue = 0;
			break;
		}
		case (2):
		{
			initialValue = 0.f;
			break;
		}
		}

		Uniforms[NewUniformNameBuf] = initialValue;
	}

	std::vector<std::string> uniformsArray;
	uniformsArray.reserve(Uniforms.size());

	for (auto& it : Uniforms)
		uniformsArray.push_back(it.first);

	ImGui::ListBox(
		"Uniforms",
		Selection,
		&mtlUniformIterator,
		&uniformsArray,
		static_cast<int>(Uniforms.size())
	);

	if (*Selection != -1 && uniformsArray.size() >= 1)
	{
		const std::string& name = uniformsArray.at(*Selection);
		Reflection::GenericValue& value = Uniforms.at(name);

		static std::string NameEditBuf;
		NameEditBuf = name;

		ImGui::InputText("Name", &NameEditBuf);

		Reflection::GenericValue newValue = value;

		switch (value.Type)
		{
		case (Reflection::ValueType::Bool):
		{
			bool curVal = value.AsBool();
			ImGui::Checkbox("Value", &curVal);

			newValue = curVal;
			break;
		}
		case (Reflection::ValueType::Integer):
		{
			int32_t curVal = static_cast<int32_t>(value.AsInteger());
			ImGui::InputInt("Value", &curVal);

			newValue = curVal;
			break;
		}
		case (Reflection::ValueType::Double):
		{
			float curVal = static_cast<float>(value.AsDouble());
			ImGui::InputFloat("Value", &curVal);

			newValue = curVal;
			break;
		}
		default:
		{
			ImGui::Text(
				"<Type ID:%i ('%s') editing not supported>",
				(int)value.Type,
				Reflection::TypeAsString(value.Type).c_str()
			);
			break;
		}
		}

		if (name != NameEditBuf)
			Uniforms.erase(name);

		Uniforms[NameEditBuf] = newValue;
	}
}

static std::string* PipelineShaderSelectTarget = nullptr;

static void shaderPipelineShaderSelect(const std::string& Label, std::string* Target)
{
	ImGui::Text(Label.c_str());
	ImGui::SameLine();

	bool editFile = ImGui::TextLink(Target->c_str());
	ImGui::SetItemTooltip("View file");

	if (editFile)
		invokeTextEditor(*Target);

	ImGui::SameLine();

	ImGui::PushID(Label.c_str());

	bool changeFile = ImGui::Button("...");
	ImGui::SetItemTooltip("Select file");

	ImGui::PopID();

	if (changeFile)
	{
		std::string shddir = getFileDirectory(*Target);

		ImGuiFD::OpenDialog(
			"Select Pipeline Shader",
			ImGuiFDMode_LoadFile,
			shddir.c_str(),
			"*.vert,*.frag,*.geom",
			0,
			1
		);

		PipelineShaderSelectTarget = Target;
	}
}

static void renderShaderPipelinesEditor()
{
	ZoneScopedC(tracy::Color::DarkSeaGreen);

	if (ImGuiFD::BeginDialog("Select Pipeline Shader"))
	{
		if (!PipelineShaderSelectTarget)
			throw("yada yada");

		if (ImGuiFD::ActionDone())
		{
			if (ImGuiFD::SelectionMade())
			{
				std::string fullpath = ImGuiFD::GetSelectionPathString(0);
				size_t resDirOffset = fullpath.find("resources/");

				if (resDirOffset == std::string::npos)
				{
					ErrorTooltipMessage = "Selection must be within the Project's `resources/` directory!";
					ErrorTooltipTimeRemaining = 5.f;
				}
				else
				{
					std::string shortpath = fullpath.substr(resDirOffset + 10);
					*PipelineShaderSelectTarget = shortpath;
					// maybe `*PipelineShaderSelectTarget = shortpath` can also be done but idk
					// if that calls the copy constructor
					//PipelineShaderSelectTarget->swap(shortpath);

					PipelineShaderSelectTarget = nullptr;
				}
			}

			ImGuiFD::CloseCurrentDialog();
		}

		ImGuiFD::EndDialog();
	}

	ShaderManager* shdManager = ShaderManager::Get();

	std::vector<ShaderProgram>& shaders = shdManager->GetLoadedShaders();

	if (!ImGui::Begin("Shader Pipelines"))
	{
		ImGui::End();

		return;
	}

	static std::string NewShdName = "";
	NewShdName.reserve(64);

	ImGui::InputText("New Blank Pipeline", &NewShdName);

	if (ImGui::Button("Create"))
	{
		ShaderProgram np;
		np.Name = NewShdName;
		np.VertexShader = "worldUber.vert";
		np.FragmentShader = "worldUber.frag";
		
		np.Save();

		shdManager->LoadFromPath(NewShdName);
	}

	static int curItemIdx = -1;

	ImGui::ListBox(
		"Loaded Pipelines",
		&curItemIdx,
		[](void* udat, int idx)
		-> const char*
		{
			return ((std::vector<ShaderProgram>*)udat)->at(idx).Name.c_str();
		},
		&shaders,
		static_cast<int>(shaders.size())
	);

	if (curItemIdx != -1)
	{
		ShaderProgram& curItem = shaders[curItemIdx];

		shaderPipelineShaderSelect("Vertex Shader: ", &curItem.VertexShader);
		shaderPipelineShaderSelect("Fragment Shader: ", &curItem.FragmentShader);
		shaderPipelineShaderSelect("Geometry Shader: ", &curItem.GeometryShader);

		static int UniformSelectionIdx = -1;

		ImGui::Text("Inherit variables of: ");
		ImGui::SameLine();

		curItem.UniformsAncestor.reserve(64);

		ImGui::InputText("##", &curItem.UniformsAncestor);

		uniformsEditor(curItem.DefaultUniforms, &UniformSelectionIdx);

		if (ImGui::Button("Save & Reload"))
		{
			curItem.Save();
			curItem.Reload();
		}
	}

	ImGui::End();
}

static char* MtlEditorTextureSelectDialogBuffer = nullptr;
static uint32_t* MtlEditorTextureSelectTarget = nullptr;

static void mtlEditorTexture(uint32_t* TextureIdPtr, const char* Label, char* Buffer)
{
	TextureManager* texManager = TextureManager::Get();

	const Texture& tx = texManager->GetTextureResource(*TextureIdPtr);

	ImGui::Text(Label);

	if (ImGui::GetIO().KeyCtrl)
	{
		bool fileDialogRequested = ImGui::TextLink(Buffer);
		ImGui::SetItemTooltip("Open file dialog");

		if (fileDialogRequested)
		{
			std::string texdir = getFileDirectory(Buffer);

			ImGuiFD::OpenDialog(
				"Select Texture",
				ImGuiFDMode_LoadFile,
				texdir.c_str(),
				"*.png,*.jpg,*.jpeg",
				0,
				1
			);

			MtlEditorTextureSelectDialogBuffer = Buffer;
			MtlEditorTextureSelectTarget = TextureIdPtr;
		}
	}
	else
	{
		ImGui::InputText(Label, Buffer, MATERIAL_TEXTUREPATH_BUFSIZE);
		ImGui::SetItemTooltip("Enter path directly, or hold CTRL to use a file dialog");
	}

	ImGui::Image(
		tx.GpuId,
		// Scale to 256 pixels wide, while maintaining aspect ratio
		ImVec2(256.f, tx.Height * (256.f / tx.Width))
	);

	ImGui::Text(std::vformat(
		"Resolution: {}x{}",
		std::make_format_args(tx.Width, tx.Height)
	).c_str());

	ImGui::Text(std::vformat(
		"# Color channels: {}",
		std::make_format_args(tx.NumColorChannels)
	).c_str());
}

static uint32_t getClassIconId(const std::string& ClassName)
{
	std::string iconPath = ClassIcons["FallbackIcon"];
	auto wantedIconIt = ClassIcons.find(ClassName);

	if (wantedIconIt != ClassIcons.end())
		iconPath = wantedIconIt->second;

	return TextureManager::Get()->LoadTextureFromPath(iconPath);
}

// 02/09/2024
// That one Gianni Matragrano shitpost where it's a
// baguette with a doge face on a white background low-res
// with the caption "pain"
// and it's just him screaming into the mic
void Editor::m_RenderMaterialEditor()
{
	ZoneScopedC(tracy::Color::DarkSeaGreen);

	MaterialManager* mtlManager = MaterialManager::Get();
	TextureManager* texManager = TextureManager::Get();

	if (MtlEditorTextureSelectDialogBuffer != nullptr)
	{
		if (ImGuiFD::BeginDialog("Select Texture"))
		{
			if (ImGuiFD::ActionDone())
			{
				if (ImGuiFD::SelectionMade())
				{
					std::string fullpath = ImGuiFD::GetSelectionPathString(0);
					size_t resDirOffset = fullpath.find("resources/");

					if (resDirOffset == std::string::npos)
					{
						ErrorTooltipMessage = "Selection must be within the Project's `resources/` directory!";
						ErrorTooltipTimeRemaining = 5.f;
						MtlEditorTextureSelectDialogBuffer = nullptr;
					}
					else
					{
						std::string shortpath = fullpath.substr(resDirOffset + 10);
						CopyStringToBuffer(MtlEditorTextureSelectDialogBuffer, MATERIAL_TEXTUREPATH_BUFSIZE, shortpath);
						MtlEditorTextureSelectDialogBuffer = nullptr;

						uint32_t newtexid = texManager->LoadTextureFromPath(shortpath);
						// i'm so silly 04/12/2024
						*MtlEditorTextureSelectTarget = newtexid;
					}
				}

				ImGuiFD::CloseCurrentDialog();
				MtlEditorTextureSelectDialogBuffer = nullptr;
			}

			ImGuiFD::EndDialog();
		}
	}

	if (!ImGui::Begin("Materials"))
	{
		ImGui::End();
		return;
	}

	ImGui::InputText("Load material", m_MtlLoadNameBuf, MATERIAL_NEW_NAME_BUFSIZE);
	ImGui::SetItemTooltip("The name of the material to load in, NOT the file path");

	if (ImGui::Button("Load"))
		mtlManager->LoadMaterialFromPath(m_MtlLoadNameBuf);

	ImGui::InputText("New blank material", m_MtlCreateNameBuf, MATERIAL_NEW_NAME_BUFSIZE);
	ImGui::SetItemTooltip("The name of the new blank material");

	if (ImGui::Button("Create"))
	{
		FileRW::WriteFile(
			"materials/" + std::string(m_MtlCreateNameBuf) + ".mtl",
			DefaultNewMaterial.dump(2), true
		);

		mtlManager->LoadMaterialFromPath(m_MtlCreateNameBuf);
	}

	std::vector<RenderMaterial>& loadedMaterials = mtlManager->GetLoadedMaterials();

	ImGui::ListBox(
		"Loaded materials",
		&m_MtlCurItem,
		&mtlIterator,
		nullptr,
		static_cast<int>(loadedMaterials.size())
	);
	ImGui::SetItemTooltip("Use the 'Load' button to load a material if it isn't already loaded");

	if (m_MtlCurItem == -1)
	{
		ImGui::End();

		return;
	}

	RenderMaterial& curItem = loadedMaterials.at(m_MtlCurItem);

	Texture& colorMap = texManager->GetTextureResource(curItem.ColorMap);
	Texture& metallicRoughnessMap = texManager->GetTextureResource(curItem.MetallicRoughnessMap);
	Texture& normalMap = texManager->GetTextureResource(curItem.NormalMap);
	Texture& emissionMap = texManager->GetTextureResource(curItem.EmissionMap);

	static int SelectedUniformIdx = -1;

	static std::string s_SaveNameBuf = "";

	if (m_MtlCurItem != m_MtlPrevItem)
	{
		CopyStringToBuffer(m_MtlShpBuf, MATERIAL_TEXTUREPATH_BUFSIZE, curItem.GetShader().Name);
		s_SaveNameBuf = curItem.Name;

		CopyStringToBuffer(m_MtlDiffuseBuf, MATERIAL_TEXTUREPATH_BUFSIZE, colorMap.ImagePath);
		CopyStringToBuffer(m_MtlSpecBuf, MATERIAL_TEXTUREPATH_BUFSIZE, metallicRoughnessMap.ImagePath);
		CopyStringToBuffer(m_MtlNormalBuf, MATERIAL_TEXTUREPATH_BUFSIZE, normalMap.ImagePath);
		CopyStringToBuffer(m_MtlEmissionBuf, MATERIAL_TEXTUREPATH_BUFSIZE, emissionMap.ImagePath);

		SelectedUniformIdx = -1;
	}

	m_MtlPrevItem = m_MtlCurItem;

	ImGui::Image(
		MtlEditorPreview.GpuTextureId,
		ImVec2(256.f, 256.f),
		ImVec2(0.f, 1.f),
		ImVec2(1.f, 0.f)
	);

	static double PreviewRotStart = 0.f;

	if (ImGui::IsItemHovered())
	{
		if (PreviewRotStart == 0.f)
			PreviewRotStart = GetRunningTime();

		glm::mat4 transform = MtlPreviewCamDefaultRotation * glm::rotate(
			glm::mat4(1.f),
			glm::radians(static_cast<float>((GetRunningTime() - PreviewRotStart) * 45.f)),
			glm::vec3(0.f, 1.f, 0.f)
		);
		MtlPreviewCamera->Transform = transform * MtlPreviewCamOffset;
	}
	else
	{
		MtlPreviewCamera->Transform = MtlPreviewCamDefaultRotation * MtlPreviewCamOffset;
		PreviewRotStart = 0.f;
	}

	const GpuFrameBuffer& prevFbo = MtlPreviewRenderer->FrameBuffer;
	//MtlPreviewRenderer->FrameBuffer = MtlEditorPreview;

	MtlEditorPreview.Bind();
	glViewport(0, 0, 256, 256);
	glClearColor(0.3f, 0.78f, 1.f, 1.f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	static uint32_t CubeMeshId = MeshProvider::Get()->LoadFromPath("!Cube");

	MtlPreviewScene.RenderList[0].MaterialId = static_cast<uint32_t>(m_MtlCurItem);
	MtlPreviewScene.RenderList[0].RenderMeshId = CubeMeshId;
	MtlPreviewScene.UsedShaders = { curItem.ShaderId };

	MtlPreviewRenderer->DrawScene(
		MtlPreviewScene,
		MtlPreviewCamera->GetMatrixForAspectRatio(1.f),
		MtlPreviewCamera->Transform,
		GetRunningTime()
	);

	MtlEditorPreview.Unbind();
	MtlPreviewRenderer->FrameBuffer = prevFbo;
	MtlPreviewRenderer->FrameBuffer.Bind();
	glViewport(0, 0, prevFbo.Width, prevFbo.Height);

	ImGui::InputText("Shader", m_MtlShpBuf, MATERIAL_TEXTUREPATH_BUFSIZE);

	mtlEditorTexture(&curItem.ColorMap, "Color Map:", m_MtlDiffuseBuf);

	bool hadSpecularTexture = curItem.MetallicRoughnessMap != 0;
	bool metallicRoughnessEnabled = hadSpecularTexture;
	ImGui::Checkbox("Has Metallic-Roughness Map", &metallicRoughnessEnabled);

	if (metallicRoughnessEnabled)
	{
		if (!hadSpecularTexture)
			curItem.MetallicRoughnessMap = texManager->LoadTextureFromPath("textures/white.png");

		mtlEditorTexture(&curItem.MetallicRoughnessMap, "Metallic Roughness Map:", m_MtlSpecBuf);
	}
	else
		// the ID is the only thing the Renderer uses to determine
		// if a material has a certain non-required texture,
		// as well as being what this very Editor uses to determine if it
		// should be displayed
		// 15/11/2024
		curItem.MetallicRoughnessMap = 0;

	bool hadNormalMap = curItem.NormalMap != 0;
	bool normalMapEnabled = hadNormalMap;
	ImGui::Checkbox("Has Normal Map", &normalMapEnabled);

	if (normalMapEnabled)
	{
		if (!hadNormalMap)
			curItem.NormalMap = texManager->LoadTextureFromPath("textures/violet.png");

		mtlEditorTexture(&curItem.NormalMap, "Normal Map:", m_MtlNormalBuf);
	}
	else
		curItem.NormalMap = 0;

	bool hadEmissiveMap = curItem.EmissionMap != 0;
	bool emissiveMapEnabled = hadEmissiveMap;
	ImGui::Checkbox("Has Emission Map", &emissiveMapEnabled);

	if (emissiveMapEnabled)
	{
		if (!hadEmissiveMap)
			curItem.EmissionMap = texManager->LoadTextureFromPath("textures/white.png");

		mtlEditorTexture(&curItem.EmissionMap, "Emission Map:", m_MtlEmissionBuf);
	}
	else
		curItem.EmissionMap = 0;

	ImGui::Text("Shader Variable Overrides");

	uniformsEditor(curItem.Uniforms, &SelectedUniformIdx);

	ImGui::Checkbox("Has translucency", &curItem.HasTranslucency);
	ImGui::InputFloat("Spec pow", &curItem.SpecExponent);
	ImGui::InputFloat("Spec mul", &curItem.SpecMultiply);

	ImGui::InputText("Save As", &s_SaveNameBuf, MATERIAL_NEW_NAME_BUFSIZE);
	ImGui::SetItemTooltip("By default, this will be the name of the material you are editing, thus overwriting it");

	if (ImGui::Button("Save changes"))
	{
		curItem.ColorMap = texManager->LoadTextureFromPath(m_MtlDiffuseBuf);

		if (curItem.MetallicRoughnessMap != 0)
			curItem.MetallicRoughnessMap = texManager->LoadTextureFromPath(m_MtlSpecBuf);

		if (curItem.NormalMap != 0)
			curItem.NormalMap = texManager->LoadTextureFromPath(m_MtlNormalBuf);

		if (curItem.EmissionMap != 0)
			curItem.EmissionMap = texManager->LoadTextureFromPath(m_MtlEmissionBuf);

		curItem.ShaderId = ShaderManager::Get()->LoadFromPath(m_MtlShpBuf);

		mtlManager->SaveToPath(curItem, s_SaveNameBuf);
	}

	ImGui::End();
}

static uint32_t HierarchyTreeSelectionId = PHX_GAMEOBJECT_NULL_ID;
static GameObject* ObjectInsertionTarget = nullptr;

static GameObject* recursiveIterateTree(GameObject* current, bool didVisitCurSelection = false)
{
	ZoneScopedC(tracy::Color::DarkSeaGreen);

	static TextureManager* texManager = TextureManager::Get();

	GameObject* hrchSelection = GameObject::GetObjectById(HierarchyTreeSelectionId);

	if (hrchSelection == nullptr)
		HierarchyTreeSelectionId = PHX_GAMEOBJECT_NULL_ID;

	// https://github.com/ocornut/imgui/issues/581#issuecomment-216054349
	// 07/10/2024
	GameObject* nodeClicked = nullptr;

	if (current->ObjectId == HierarchyTreeSelectionId)
		didVisitCurSelection = true;

	static GameObject* InsertObjectButtonHoveredOver = nullptr;
	InsertObjectButtonHoveredOver = nullptr;

	for (GameObject* object : current->GetChildren())
	{
		if (object == nullptr)
			throw("stoopid compiler is giving me a warning for something that will probably not happen");

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_SpanAvailWidth;

		// make the insert button have better contrast
		if (hrchSelection && object == hrchSelection && object != InsertObjectButtonHoveredOver)
			flags |= ImGuiTreeNodeFlags_Selected;

		if (object->GetChildren().empty())
			flags |= ImGuiTreeNodeFlags_Leaf;

		ImGui::AlignTextToFramePadding();

		ImGui::Image(
			texManager->GetTextureResource(getClassIconId(object->ClassName)).GpuId,
			ImVec2(16.f, 16.f)
		);
		ImGui::SameLine();

		if (!didVisitCurSelection)
		{
			std::vector<GameObject*> descs = object->GetDescendants();

			if (std::find(descs.begin(), descs.end(), hrchSelection) != descs.end())
				ImGui::SetNextItemOpen(true);
		}

		bool open = ImGui::TreeNodeEx(&object->ObjectId, flags, object->Name.c_str());

		if (ImGui::IsItemClicked())
			nodeClicked = object;

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)
			&& ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
		)
		{
			ImGui::OpenPopup(1979);
			HierarchyTreeSelectionId = object->ObjectId;
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SameLine();
			ImGui::Button("+");

			// the above call to `::Button` will always
			// return false, ig this does something different
			// with the ordering 15/12/2024
			if (ImGui::IsItemClicked())
			{
				ObjectInsertionTarget = object;
				ImGui::OpenPopup("Object Insertion Window");
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetItemTooltip("Insert new Object");
				InsertObjectButtonHoveredOver = object;
			}
		}

		if (object == ObjectInsertionTarget)
		{
			if (ImGui::BeginPopup("Object Insertion Window"))
			{
				ImGui::SeparatorText("Insert");

				for (auto& it : GameObject::s_GameObjectMap)
					if (ImGui::MenuItem(it.first.c_str()))
					{
						GameObject* newObject = GameObject::Create(it.first);
						newObject->SetParent(ObjectInsertionTarget);
						HierarchyTreeSelectionId = newObject->ObjectId;
						
						ObjectInsertionTarget = nullptr;
					}

				InsertObjectButtonHoveredOver = nullptr;

				ImGui::EndPopup();
			}
		}
			
		if (open)
		{
			recursiveIterateTree(object, didVisitCurSelection);
			ImGui::TreePop();
		}
	}

	if (nodeClicked)
		if (ImGui::GetIO().KeyCtrl)
			HierarchyTreeSelectionId = PHX_GAMEOBJECT_NULL_ID;
		else
			HierarchyTreeSelectionId = nodeClicked->ObjectId;

	return GameObject::GetObjectById(HierarchyTreeSelectionId);
}

static std::string DoNotShowPropThisFrame = "";

void Editor::RenderUI()
{
	ZoneScopedC(tracy::Color::DarkSeaGreen);

	if (ErrorTooltipTimeRemaining > 0.f)
		ImGui::SetTooltip(ErrorTooltipMessage.c_str());

	renderTextEditor();
	renderShaderPipelinesEditor();
	m_RenderMaterialEditor();

	if (!ImGui::Begin("Editor"))
	{
		ImGui::End();
		return;
	}

	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(227, 227, 227, 255));
	ImGui::Text("Compiled: %s", __DATE__);
	ImGui::PopStyleColor();

	ImVec2 hrchChildWinSzOverride{};

	if (HierarchyTreeSelectionId != PHX_GAMEOBJECT_NULL_ID)
		hrchChildWinSzOverride = ImGui::GetContentRegionAvail() * ImVec2(1.f, .4f);

	ImGui::BeginChild("HierarchyChildWindow", hrchChildWinSzOverride, ImGuiChildFlags_Border);

	GameObject* selected = recursiveIterateTree(GameObject::s_DataModel);
	
	GameObjectRef<GameObject>* selRef = nullptr;

	if (selected)
		selRef = new GameObjectRef<GameObject>(selected);

	if (ImGui::BeginPopupEx(1979, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::SeparatorText("Actions");

		GameObject* target = GameObject::GetObjectById(HierarchyTreeSelectionId);

		if (!target)
		{
			HierarchyTreeSelectionId = PHX_GAMEOBJECT_NULL_ID;
			ImGui::CloseCurrentPopup();
		}
		else
		{
			if (Object_Script* scr = dynamic_cast<Object_Script*>(target))
			{
				if (ImGui::MenuItem("Edit"))
					invokeTextEditor(scr->SourceFile);

				if (ImGui::MenuItem("Reload"))
					scr->Reload();
			}

			if (ImGui::MenuItem("Duplicate"))
			{
				GameObject* dup = target->Duplicate();
				HierarchyTreeSelectionId = dup->ObjectId;
			}

			if (ImGui::MenuItem("Delete"))
				target->Destroy();
		}

		ImGui::EndPopup();
	}

	ImGui::EndChild();

	if (selected)
	{
		ImGui::BeginChild("PropertiesEditor", ImVec2(), ImGuiChildFlags_Border);

		std::string sepStr = std::vformat(
			"Properties of {} '{}'",
			std::make_format_args(selected->ClassName, selected->Name)
		);
		ImGui::SeparatorText(sepStr.c_str());

		auto& props = selected->GetProperties();

		for (auto& propListItem : props)
		{
			const char* propName = propListItem.first.c_str();
			const Reflection::Property& prop = propListItem.second;

			if (propName == DoNotShowPropThisFrame)
			{
				// force Dear ImGui to lose focus of this specific property
				// this is for CTRL+Click'ing `.Parent` so the value in the input
				// box doesn't get carried over to the Object we jump to, forcing
				// it's `.Parent` to change 15/12/2024
				DoNotShowPropThisFrame = "";
				continue;
			}

			Reflection::GenericValue curVal = selected->GetPropertyValue(propName);

			if (!prop.Set)
			{
				// no setter (locked property, such as ClassName or ObjectId)
				// 07/07/2024

				std::string curValStr = curVal.ToString();

				if (strcmp(propName, "Class") == 0)
				{
					ImGui::Text("Class: ");
					ImGui::SameLine();

					ImGui::Image(
						TextureManager::Get()->GetTextureResource(getClassIconId(selected->ClassName)).GpuId,
						ImVec2(16, 16)
					);
					ImGui::SameLine();

					ImGui::Text(curValStr.c_str());
				}
				else
					if (curVal.Type == Reflection::ValueType::Matrix)
					{
						ImGui::Text("%s: ", propName);

						ImGui::Indent();

						curValStr.insert(curValStr.begin() + curValStr.find_first_of("Ang"), '\n');
						ImGui::Text(curValStr.c_str());

						ImGui::Unindent();
					}
					else
						ImGui::Text("%s: %s", propName, curValStr.c_str());

				continue;
			}

			Reflection::GenericValue newVal = curVal;

			switch (curVal.Type)
			{

			case (Reflection::ValueType::String):
			{
				std::string str = curVal.AsString();

				const size_t INPUT_TEXT_BUFFER_ADDITIONAL = 64;

				size_t allocSize = str.size() + INPUT_TEXT_BUFFER_ADDITIONAL;

				char* buf = BufferInitialize(
					allocSize,
					"<Initial Value 29/09/2024 Hey guys How we doing today>"
				);

				memcpy(buf, str.data(), str.size());

				buf[str.size()] = 0;

				ImGui::InputText(propName, buf, allocSize);
				newVal = std::string(buf);

				Memory::Free(buf);

				break;
			}

			case (Reflection::ValueType::Bool):
			{
				bool b = newVal.AsBool();

				ImGui::Checkbox(propName, &b);
				newVal = b;

				break;
			}

			case (Reflection::ValueType::Double):
			{
				double d = newVal.AsDouble();

				ImGui::InputDouble(propName, &d);
				newVal = d;

				break;
			}

			case (Reflection::ValueType::Integer):
			{
				// TODO BIG BAD HACK HACK HACK
				// stoobid Dear ImGui :'(
				// only allows 32-bit integer input
				// 01/09/2024
				int32_t valAs32Bit = static_cast<int32_t>(curVal.AsInteger());

				ImGui::InputInt(propName, &valAs32Bit);

				newVal = valAs32Bit;

				break;
			}

			case (Reflection::ValueType::GameObject):
			{
				// TODO BIG BAD HACK HACK HACK
				// stoobid Dear ImGui :'(
				// only allows 32-bit integer input
				// 01/09/2024
				int32_t id = static_cast<int32_t>(curVal.AsInteger());

				ImGui::InputInt(propName, &id);
				ImGui::SetItemTooltip("CTRL+Click to select referenced GameObject 03/12/2024");

				if (ImGui::IsItemClicked())
					if (ImGui::GetIO().KeyCtrl)
					{
						HierarchyTreeSelectionId = static_cast<uint32_t>(curVal.AsInteger());
						DoNotShowPropThisFrame = propName;
					}

				newVal = id;
				newVal.Type = Reflection::ValueType::GameObject;

				break;
			}

			case (Reflection::ValueType::Color):
			{
				Color col = curVal;

				float entry[3] = { col.R, col.G, col.B };

				ImGui::ColorEdit3(propName, entry, ImGuiColorEditFlags_None);

				//ImGui::InputFloat3(propName, entry);

				col.R = entry[0];
				col.G = entry[1];
				col.B = entry[2];

				newVal = col.ToGenericValue();

				break;
			}

			case (Reflection::ValueType::Vector3):
			{
				Vector3 vec = curVal;

				float entry[3] =
				{
					static_cast<float>(vec.X),
					static_cast<float>(vec.Y),
					static_cast<float>(vec.Z)
				};

				ImGui::InputFloat3(propName, entry);

				vec.X = entry[0];
				vec.Y = entry[1];
				vec.Z = entry[2];

				newVal = vec.ToGenericValue();

				break;
			}

			case (Reflection::ValueType::Matrix):
			{
				glm::mat4 mat = curVal.AsMatrix();

				ImGui::Text("%s:", propName);

				float pos[3] =
				{
					mat[3][0],
					mat[3][1],
					mat[3][2]
				};

				// PLEASE GOD JUST WORK ALREADY
				// 21/09/2024
				glm::vec3 rotrads{};

				glm::extractEulerAngleXYZ(mat, rotrads.x, rotrads.y, rotrads.z);

				//mat = glm::rotate(mat, -rotrads[0], glm::vec3(1.f, 0.f, 0.f));
				//mat = glm::rotate(mat, -rotrads[1], glm::vec3(0.f, 1.f, 0.f));
				//mat = glm::rotate(mat, -rotrads[2], glm::vec3(0.f, 0.f, 1.f));

				float rotdegs[3] =
				{
					glm::degrees(rotrads.x),
					glm::degrees(rotrads.y),
					glm::degrees(rotrads.z)
				};

				ImGui::Indent();

				ImGui::InputFloat3("Position", pos);
				ImGui::InputFloat3("Rotation", rotdegs);

				ImGui::Unindent();

				mat = glm::mat4(1.f);

				mat[3][0] = pos[0];
				mat[3][1] = pos[1];
				mat[3][2] = pos[2];

				mat *= glm::eulerAngleXYZ(glm::radians(rotdegs[0]), glm::radians(rotdegs[1]), glm::radians(rotdegs[2]));

				newVal = Reflection::GenericValue(mat);

				break;
			}

			default:
			{
				int typeId = static_cast<int>(curVal.Type);
				const std::string& typeName = Reflection::TypeAsString(curVal.Type);

				ImGui::Text(std::vformat(
					"{}: <Display of ID:{} ('{}') types not unavailable>",
					std::make_format_args(propName, typeId, typeName)
				).c_str());

				break;
			}

			}

			try
			{
				selected->SetPropertyValue(propName, newVal);
			}
			catch (std::string err)
			{
				ErrorTooltipMessage = err;
				ErrorTooltipTimeRemaining = 2.f;
			}
		}

		ImGui::EndChild();
	}

	ImGui::End();

	delete selRef;
}
