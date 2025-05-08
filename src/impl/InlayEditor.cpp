#define GLM_ENABLE_EXPERIMENTAL
#define IMGUI_DEFINE_MATH_OPERATORS

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_internal.h>
#include <tracy/Tracy.hpp>
#include <glad/gl.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_dialog.h>
#include <fstream>
#include <set>

#include "InlayEditor.hpp"

#include "asset/MaterialManager.hpp"
#include "asset/TextureManager.hpp"
#include "asset/MeshProvider.hpp"

#include "component/Camera.hpp"
#include "component/Script.hpp"

#include "Utilities.hpp"
#include "UserInput.hpp"
#include "Memory.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

constexpr uint32_t OBJECT_NEW_CLASSNAME_BUFSIZE = 16;
constexpr uint32_t MATERIAL_NEW_NAME_BUFSIZE = 64;
constexpr uint32_t MATERIAL_TEXTUREPATH_BUFSIZE = 128;
constexpr char MATERIAL_NEW_NAME_DEFAULT[] = "newmaterial";

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
static GameObjectRef MtlPreviewCamera;
static glm::mat4 MtlPreviewCamOffset = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -2.f));
static glm::mat4 MtlPreviewCamDefaultRotation = glm::eulerAngleYXZ(glm::radians(168.f), glm::radians(12.f), 0.f);

static std::string ErrorTooltipMessage = "No Error Dummy";
static double ErrorTooltipTimeRemaining = 0.f;

static char MtlCreateNameBuf[MATERIAL_NEW_NAME_BUFSIZE] = "material";
static char MtlLoadNameBuf[MATERIAL_NEW_NAME_BUFSIZE] = "material";
static char MtlDiffuseBuf[MATERIAL_TEXTUREPATH_BUFSIZE] = { 0 };
static char MtlSpecBuf[MATERIAL_TEXTUREPATH_BUFSIZE] = { 0 };
static char MtlNormalBuf[MATERIAL_TEXTUREPATH_BUFSIZE] = { 0 };
static char MtlEmissionBuf[MATERIAL_TEXTUREPATH_BUFSIZE] = { 0 };
static char MtlShpBuf[MATERIAL_TEXTUREPATH_BUFSIZE] = { 0 };
static int MtlCurItem = -1;
static int MtlPrevItem = -1;

static void copyStringToBuffer(char* Buffer, const std::string_view& String, size_t BufSize)
{
	for (size_t i = 0; i < BufSize; i++)
		Buffer[i] = String.size() > i ? String.at(i) : 0;
}

void InlayEditor::Initialize(Renderer* renderer)
{
	MtlEditorPreview.Initialize(256, 256);
	MtlPreviewRenderer = renderer;
	MtlPreviewCamera = GameObject::Create("Camera");

	EcCamera* cc = MtlPreviewCamera->GetComponent<EcCamera>();
	cc->Transform = MtlPreviewCamDefaultRotation * MtlPreviewCamOffset;
	cc->FieldOfView = 50.f;

	InlayEditor::DidInitialize = true;
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
	std::vector<std::string_view>* arr = static_cast<std::vector<std::string_view>*>(array);
	*outText = arr->at(index).data();

	return true;
}

static std::string getFileDirectory(const std::string& FilePath)
{
	size_t lastFwdSlash = FilePath.find_last_of("/");

	if (lastFwdSlash == std::string::npos)
#ifdef _WIN32
		return SDL_GetCurrentDirectory() + std::string("resources\\");
#else
		return SDL_GetCurrentDirectory() + std::string("resources/");
#endif
	else
	{
		std::string fileDir = FilePath.substr(0, lastFwdSlash);

		if (fileDir.find("resources/") == std::string::npos)
			fileDir = "resources/" + fileDir;

		return SDL_GetCurrentDirectory() + fileDir;
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

			SDL_ShowOpenFileDialog(
				[](void*, const char* const* FileList, int)
				{
					if (!FileList)
						throw(SDL_GetError());
					if (!FileList[0])
						return;

					TextEditorFile = FileList[0];

					// windows SMELLS :( 18/02/2025
					size_t off = TextEditorFile.find_first_of("\\");

					while (off != std::string::npos)
						off = TextEditorFile.replace(off, 1, "/").find_first_of("\\");

					if (!TextEditorFile.find("resources/"))
						TextEditorFile.insert(0, "./"); // for `FileRW::TryMakePathCwdRelative`

					TextEditorQuickSelectFiles.insert(TextEditorFile);
				},
				nullptr,
				SDL_GL_GetCurrentWindow(),
				nullptr,
				0,
				curDir.c_str(),
				false
			);
		}

		if (ImGui::MenuItem("Save", NULL, nullptr, TextEditorFile != "" && TextEditorFile != "<NOT_SELECTED>"))
			textEditorSaveFile();

		if (ImGui::MenuItem("Save As"))
		{
			std::string curDir = getFileDirectory(TextEditorFile);

			SDL_ShowSaveFileDialog(
				[](void*, const char* const* FileList, int)
				{
					if (!FileList)
						throw(SDL_GetError());
					if (!FileList[0])
						return;

					TextEditorFile = FileList[0];

					// windows SMELLS :( 18/02/2025
					size_t off = TextEditorFile.find_first_of("\\");

					while (off != std::string::npos)
						off = TextEditorFile.replace(off, 1, "/").find_first_of("\\");

					if (!TextEditorFile.find("resources/"))
						TextEditorFile.insert(0, "./"); // for `FileRW::TryMakePathCwdRelative`

					textEditorSaveFile();

					TextEditorQuickSelectFiles.insert(TextEditorFile);
				},
				nullptr,
				SDL_GL_GetCurrentWindow(),
				nullptr,
				0,
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

	if (!TextEditorEntryBuffer)
	{
		if (TextEditorFileStream)
		{
			TextEditorFileStream->close();
			delete TextEditorFileStream;
		}

		TextEditorFileStream = new std::fstream(FileRW::TryMakePathCwdRelative(TextEditorFile));
		
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

			copyStringToBuffer(TextEditorEntryBuffer, scriptContents, TextEditorEntryBufferCapacity);
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

	static int NewTypeId = 0;

	static std::string NewUniformNameBuf;
	NewUniformNameBuf.resize(32);

	ImGui::InputText("Variable Name", &NewUniformNameBuf);

	ImGui::Combo("Variable Type", &NewTypeId, "Boolean\0Integer\0Float");

	if (ImGui::Button("Add"))
	{
		Reflection::GenericValue initialValue;

		switch (NewTypeId)
		{
		case 0:
		{
			initialValue = true;
			break;
		}
		case 1:
		{
			initialValue = 0;
			break;
		}
		case 2:
		{
			initialValue = 0.f;
			break;
		}
		default:
			assert(false);
		}

		Uniforms[NewUniformNameBuf] = initialValue;
	}

	std::vector<const char*> uniformsArray;
	uniformsArray.reserve(Uniforms.size());

	for (auto& it : Uniforms)
		uniformsArray.push_back(it.first.c_str());

	ImGui::ListBox("Uniforms", Selection, uniformsArray.data(), static_cast<int>(uniformsArray.size()));

	if (*Selection != -1 && uniformsArray.size() >= 1)
	{
		const std::string& name = uniformsArray.at(*Selection);
		Reflection::GenericValue value = Uniforms[name];

		static std::string NameEditBuf;
		NameEditBuf = name;

		ImGui::InputText("Name", &NameEditBuf);

		Reflection::GenericValue newValue = value;

		switch (value.Type)
		{
		case Reflection::ValueType::Boolean:
		{
			bool curVal = value.AsBoolean();
			ImGui::Checkbox("Value", &curVal);

			newValue = curVal;
			break;
		}
		case Reflection::ValueType::Integer:
		{
			int32_t curVal = static_cast<int32_t>(value.AsInteger());
			ImGui::InputInt("Value", &curVal);

			newValue = curVal;
			break;
		}
		case Reflection::ValueType::Double:
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
				Reflection::TypeAsString(value.Type).data()
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
	ImGui::TextUnformatted(Label.c_str());
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
		SDL_DialogFileFilter filter{ "Shader files", "vert;frag;geom" };

		SDL_ShowOpenFileDialog(
			[](void*, const char* const* FileList, int)
			{
				if (!FileList)
					throw(SDL_GetError());
				if (!FileList[0])
					return;

				std::string fullpath = FileList[0];

				// windows SMELLS :( 18/02/2025
				size_t off = fullpath.find_first_of("\\");

				while (off != std::string::npos)
					off = fullpath.replace(off, 1, "/").find_first_of("\\");

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

					PipelineShaderSelectTarget = nullptr;
				}
			},
			nullptr,
			SDL_GL_GetCurrentWindow(),
			&filter,
			1,
			shddir.c_str(),
			false
		);

		PipelineShaderSelectTarget = Target;
	}
}

static void renderShaderPipelinesEditor()
{
	ZoneScopedC(tracy::Color::DarkSeaGreen);

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

static void mtlEditorTexture(const char* Label, uint32_t* TextureIdPtr, char* CurrentPath, bool CanRemove = true)
{
	ImGui::PushID(TextureIdPtr);

	ImGui::TextUnformatted(Label);
	ImGui::SameLine();

	TextureManager* texManager = TextureManager::Get();

	bool addTextureDialog = false;

	if (*TextureIdPtr == 0)
		if (ImGui::Button("Add"))
		{
			addTextureDialog = true;
			memcpy(CurrentPath, (char*)("textures/"), 9); // start the file dialog in the textures directory
		}
		else
		{
			ImGui::PopID();
			return;
		}

	const Texture& tx = texManager->GetTextureResource(*TextureIdPtr);

	if (ImGui::GetIO().KeyCtrl || addTextureDialog)
	{
		bool fileDialogRequested = addTextureDialog ? true : ImGui::TextLink(CurrentPath);
		ImGui::SetItemTooltip("Open file dialog");

		if (fileDialogRequested)
		{
			std::string texdir = getFileDirectory(CurrentPath);

			SDL_DialogFileFilter filter{ "Images", "png;jpg;jpeg" };

			SDL_ShowOpenFileDialog(
				[](void*, const char* const* FileList, int)
				{
					if (!FileList)
						throw(SDL_GetError());
					if (!FileList[0])
						return;

					std::string fullpath = FileList[0];

					// windows SMELLS :( 18/02/2025
					size_t off = fullpath.find_first_of("\\");

					while (off != std::string::npos)
						off = fullpath.replace(off, 1, "/").find_first_of("\\");

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
						copyStringToBuffer(MtlEditorTextureSelectDialogBuffer, shortpath, MATERIAL_TEXTUREPATH_BUFSIZE);
						MtlEditorTextureSelectDialogBuffer = nullptr;

						uint32_t newtexid = TextureManager::Get()->LoadTextureFromPath(shortpath);
						// i'm so silly 04/12/2024
						*MtlEditorTextureSelectTarget = newtexid;
					}
				},
				nullptr,
				SDL_GL_GetCurrentWindow(),
				&filter,
				1,
				texdir.c_str(),
				false
			);

			MtlEditorTextureSelectDialogBuffer = CurrentPath;
			MtlEditorTextureSelectTarget = TextureIdPtr;
		}

		if (addTextureDialog)
		{
			ImGui::PopID();
			return;
		}
	}
	else
	{
		CurrentPath[MATERIAL_TEXTUREPATH_BUFSIZE - 1] = 0;
		ImGui::InputText("##", CurrentPath, MATERIAL_TEXTUREPATH_BUFSIZE);
		ImGui::SetItemTooltip("Enter path directly, or hold CTRL to use a file dialog");
	}

	if (CanRemove && ImGui::Button("Remove"))
	{
		*TextureIdPtr = 0;
		ImGui::PopID();
		return;
	}

	ImGui::Image(
		tx.GpuId,
		// Scale to 256 pixels wide, while maintaining aspect ratio
		ImVec2(256.f, tx.Height * (256.f / tx.Width))
	);

	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();

		ImGui::TextUnformatted(std::vformat(
			"Resolution: {}x{}",
			std::make_format_args(tx.Width, tx.Height)
		).c_str());
		ImGui::TextUnformatted(std::vformat(
			"# Color channels: {}",
			std::make_format_args(tx.NumColorChannels)
		).c_str());

		ImGui::EndTooltip();
	}

	ImGui::PopID();
}

// 02/09/2024
// That one Gianni Matragrano shitpost where it's a
// baguette with a doge face on a white background low-res
// with the caption "pain"
// and it's just him screaming into the mic
static void renderMaterialEditor()
{
	ZoneScopedC(tracy::Color::DarkSeaGreen);

	MaterialManager* mtlManager = MaterialManager::Get();
	TextureManager* texManager = TextureManager::Get();

	if (!ImGui::Begin("Materials"))
	{
		ImGui::End();
		return;
	}

	ImGui::InputText("Load material", MtlLoadNameBuf, MATERIAL_NEW_NAME_BUFSIZE);
	ImGui::SetItemTooltip("The name of the material to load in, NOT the file path");

	if (ImGui::Button("Load"))
		mtlManager->LoadFromPath(MtlLoadNameBuf);

	ImGui::InputText("New blank material", MtlCreateNameBuf, MATERIAL_NEW_NAME_BUFSIZE);
	ImGui::SetItemTooltip("The name of the new blank material");

	if (ImGui::Button("Create"))
	{
		FileRW::WriteFile(
			"materials/" + std::string(MtlCreateNameBuf) + ".mtl",
			DefaultNewMaterial.dump(2), true
		);

		mtlManager->LoadFromPath(MtlCreateNameBuf);
	}

	std::vector<RenderMaterial>& loadedMaterials = mtlManager->GetLoadedMaterials();

	ImGui::ListBox(
		"Loaded materials",
		&MtlCurItem,
		&mtlIterator,
		nullptr,
		static_cast<int>(loadedMaterials.size())
	);
	ImGui::SetItemTooltip("Use the 'Load' button to load a material if it isn't already loaded");

	if (MtlCurItem == -1)
	{
		ImGui::End();

		return;
	}

	RenderMaterial& curItem = loadedMaterials.at(MtlCurItem);

	Texture& colorMap = texManager->GetTextureResource(curItem.ColorMap);
	Texture& metallicRoughnessMap = texManager->GetTextureResource(curItem.MetallicRoughnessMap);
	Texture& normalMap = texManager->GetTextureResource(curItem.NormalMap);
	Texture& emissionMap = texManager->GetTextureResource(curItem.EmissionMap);

	static int SelectedUniformIdx = -1;

	static std::string s_SaveNameBuf = "";

	if (MtlCurItem != MtlPrevItem)
	{
		copyStringToBuffer(MtlShpBuf, curItem.GetShader().Name, MATERIAL_TEXTUREPATH_BUFSIZE);
		s_SaveNameBuf = curItem.Name;

		copyStringToBuffer(MtlDiffuseBuf, colorMap.ImagePath, MATERIAL_TEXTUREPATH_BUFSIZE);
		copyStringToBuffer(MtlSpecBuf, metallicRoughnessMap.ImagePath, MATERIAL_TEXTUREPATH_BUFSIZE);
		copyStringToBuffer(MtlNormalBuf, normalMap.ImagePath, MATERIAL_TEXTUREPATH_BUFSIZE);
		copyStringToBuffer(MtlEmissionBuf, emissionMap.ImagePath, MATERIAL_TEXTUREPATH_BUFSIZE);

		SelectedUniformIdx = -1;
	}

	MtlPrevItem = MtlCurItem;

	ImGui::Image(
		MtlEditorPreview.GpuTextureId,
		ImVec2(256.f, 256.f),
		ImVec2(0.f, 1.f),
		ImVec2(1.f, 0.f)
	);

	static double PreviewRotStart = 0.f;

	EcCamera* cc = MtlPreviewCamera->GetComponent<EcCamera>();

	if (ImGui::IsItemHovered())
	{
		if (PreviewRotStart == 0.f)
			PreviewRotStart = GetRunningTime();

		glm::mat4 transform = MtlPreviewCamDefaultRotation * glm::rotate(
			glm::mat4(1.f),
			glm::radians(static_cast<float>((GetRunningTime() - PreviewRotStart) * 45.f)),
			glm::vec3(0.f, 1.f, 0.f)
		);
		cc->Transform = transform * MtlPreviewCamOffset;
	}
	else
	{
		cc->Transform = MtlPreviewCamDefaultRotation * MtlPreviewCamOffset;
		PreviewRotStart = 0.f;
	}

	const GpuFrameBuffer& prevFbo = MtlPreviewRenderer->FrameBuffer;
	//MtlPreviewRenderer->FrameBuffer = MtlEditorPreview;

	MtlEditorPreview.Bind();
	glViewport(0, 0, 256, 256);
	glClearColor(0.3f, 0.78f, 1.f, 1.f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	static uint32_t CubeMeshId = MeshProvider::Get()->LoadFromPath("!Cube");

	MtlPreviewScene.RenderList[0].MaterialId = static_cast<uint32_t>(MtlCurItem);
	MtlPreviewScene.RenderList[0].RenderMeshId = CubeMeshId;
	MtlPreviewScene.UsedShaders = { curItem.ShaderId };

	MtlPreviewRenderer->DrawScene(
		MtlPreviewScene,
		cc->GetMatrixForAspectRatio(1.f),
		cc->Transform,
		GetRunningTime()
	);

	MtlEditorPreview.Unbind();
	MtlPreviewRenderer->FrameBuffer = prevFbo;
	MtlPreviewRenderer->FrameBuffer.Bind();
	glViewport(0, 0, prevFbo.Width, prevFbo.Height);

	ImGui::InputText("Shader", MtlShpBuf, MATERIAL_TEXTUREPATH_BUFSIZE);

	int curPolyMode = static_cast<uint8_t>(curItem.PolygonMode);
	ImGui::Combo("Polygon Mode", &curPolyMode, "Fill\0Line\0Point");
	curItem.PolygonMode = static_cast<RenderMaterial::MaterialPolygonMode>(std::clamp(curPolyMode, 0, 2));

	mtlEditorTexture("Color Map: ", &curItem.ColorMap, MtlDiffuseBuf, false);
	mtlEditorTexture("Metallic-Rougness Map: ", &curItem.MetallicRoughnessMap, MtlSpecBuf, false);
	mtlEditorTexture("Normal Map: ", &curItem.NormalMap, MtlNormalBuf);
	mtlEditorTexture("Emission Map:", &curItem.EmissionMap, MtlEmissionBuf);

	ImGui::Text("Shader Variable Overrides");

	uniformsEditor(curItem.Uniforms, &SelectedUniformIdx);

	ImGui::Checkbox("Has translucency", &curItem.HasTranslucency);
	ImGui::InputFloat("Spec pow", &curItem.SpecExponent);
	ImGui::InputFloat("Spec mul", &curItem.SpecMultiply);

	ImGui::InputText("Save As", &s_SaveNameBuf, MATERIAL_NEW_NAME_BUFSIZE);
	ImGui::SetItemTooltip("By default, this will be the name of the material you are editing, thus overwriting it");

	if (ImGui::Button("Save changes"))
	{
		curItem.ColorMap = texManager->LoadTextureFromPath(MtlDiffuseBuf);

		if (curItem.MetallicRoughnessMap != 0)
			curItem.MetallicRoughnessMap = texManager->LoadTextureFromPath(MtlSpecBuf);

		if (curItem.NormalMap != 0)
			curItem.NormalMap = texManager->LoadTextureFromPath(MtlNormalBuf);

		if (curItem.EmissionMap != 0)
			curItem.EmissionMap = texManager->LoadTextureFromPath(MtlEmissionBuf);

		curItem.ShaderId = ShaderManager::Get()->LoadFromPath(MtlShpBuf);

		mtlManager->SaveToPath(curItem, s_SaveNameBuf);
	}

	ImGui::End();
}

static std::vector<uint32_t> Selections;
static std::vector<uint32_t> VisibleTree;
static std::vector<uint32_t> VisibleTreeWip;
static uint32_t LastSelected = UINT32_MAX;
static GameObject* ObjectInsertionTarget = nullptr;
static bool IsPickingObject = false;
static std::vector<uint32_t> PickerTargets;
static std::string PickerTargetPropName;
static std::vector<std::string_view> ContextActionsForSelection;

static std::unordered_map<std::string_view, std::function<void(void)>> ActionHandlers =
{
	{
		"Duplicate",
		[]()
		{
			std::vector<uint32_t> newSelections;
			newSelections.reserve(Selections.size());
			
			for (uint32_t selId : Selections)
				if (GameObject* g = GameObject::GetObjectById(selId))
					newSelections.push_back(g->Duplicate()->ObjectId);

			Selections = newSelections;
		}
	},
	{
		"Delete",
		[]()
		{
			for (uint32_t selId : Selections)
				if (GameObject* g = GameObject::GetObjectById(selId))
					g->Destroy();

			Selections.clear();
		}
	},
	{
		"Edit",
		[]()
		{
			for (uint32_t selId : Selections)
				if (EcScript* s = GameObject::GetObjectById(selId)->GetComponent<EcScript>())
					invokeTextEditor(s->SourceFile);
		}
	},
	{
		"Reload",
		[]()
		{
			for (uint32_t selId : Selections)
				if (EcScript* s = GameObject::GetObjectById(selId)->GetComponent<EcScript>())
					s->Reload();
		}
	}
};

static bool isInSelections(GameObject* g)
{
	return std::find(Selections.begin(), Selections.end(), g->ObjectId) != Selections.end();
}

static std::vector<std::string_view> getPossibleActionsForSelections()
{
	std::vector<std::string_view> actions =
	{
		"Duplicate",
		"Delete"
	};

	bool gotScript = false;

	for (uint32_t selId : Selections)
	{
		GameObject* g = GameObject::GetObjectById(selId);

		if (!gotScript)
			if (g->GetComponent<EcScript>())
			{
				actions.push_back("$Script");
				actions.push_back("Edit");
				actions.push_back("Reload");

				break;
			}
	}

	return actions;
}

static void onTreeItemClicked(GameObject* nodeClicked)
{
	if (IsPickingObject)
	{
		try
		{
			for (uint32_t selId : PickerTargets)
				if (GameObject* g = GameObject::GetObjectById(selId))
					g->SetPropertyValue(PickerTargetPropName, nodeClicked->ToGenericValue());
		}
		catch (std::string Err)
		{
			ErrorTooltipMessage = Err.c_str();
			ErrorTooltipTimeRemaining = 5.f;
		}

		// restore prev selections
		Selections = PickerTargets;

		IsPickingObject = false;
		PickerTargets.clear();
		PickerTargetPropName = "";

		return; // don't actually select this object
	}

	if (ImGui::GetIO().KeyCtrl)
		if (const auto& it = std::find(Selections.begin(), Selections.end(), nodeClicked->ObjectId); it != Selections.end())
			Selections.erase(it);
		else
			Selections.push_back(nodeClicked->ObjectId);

	else if (ImGui::GetIO().KeyShift)
	{
		GameObject* lastSelected = GameObject::GetObjectById(LastSelected);

		if (lastSelected && LastSelected != nodeClicked->ObjectId)
		{
			if (!isInSelections(nodeClicked))
				Selections.push_back(nodeClicked->ObjectId);

			auto start = std::find(VisibleTree.begin(), VisibleTree.end(), LastSelected);
			auto end = std::find(VisibleTree.begin(), VisibleTree.end(), nodeClicked->ObjectId);

			if (start > end)
			{
				// doesn't seem to work unless it is copied fsr
				std::vector<uint32_t>::iterator temp = end;
				end = start;
				start = temp;
			}

			// 25/01/2025 this feels weird, like surely there's a better way
			// to iterate between `start` and `end`
			for (uint32_t middleId : std::vector<uint32_t>(start, end))
				if (!isInSelections(GameObject::GetObjectById(middleId)))
					Selections.push_back(middleId);
		}
		else
			Selections = { nodeClicked->ObjectId };
	}
	else
		Selections = { nodeClicked->ObjectId };

	LastSelected = nodeClicked->ObjectId;
}

static void recursiveIterateTree(GameObject* current, bool didVisitCurSelection = false)
{
	ZoneScopedC(tracy::Color::DarkSeaGreen);

	if (IsPickingObject)
	{
		ErrorTooltipMessage = "Pick Object";
		ErrorTooltipTimeRemaining = 0.1f;
	}

	static TextureManager* texManager = TextureManager::Get();

	// https://github.com/ocornut/imgui/issues/581#issuecomment-216054349
	// 07/10/2024
	GameObject* nodeClicked = nullptr;

	if (isInSelections(current))
		didVisitCurSelection = true;

	static GameObject* InsertObjectButtonHoveredOver = nullptr;
	InsertObjectButtonHoveredOver = nullptr;

	for (GameObject* object : current->GetChildren())
	{
		if (object == nullptr)
			throw("stoopid compiler is giving me a warning for something that will probably not happen");

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
			| ImGuiTreeNodeFlags_AllowItemOverlap
			| ImGuiTreeNodeFlags_SpanAvailWidth;

		// make the insert button have better contrast
		if (isInSelections(object) && object != InsertObjectButtonHoveredOver)
			flags |= ImGuiTreeNodeFlags_Selected;

		if (object->GetChildren().empty())
			flags |= ImGuiTreeNodeFlags_Leaf;

		ImGui::AlignTextToFramePadding();

		std::pair<EntityComponent, uint32_t> primaryComponent = object->GetComponents()[0];
		if (primaryComponent.first == EntityComponent::Transform && object->GetComponents().size() > 1)
			primaryComponent = object->GetComponents()[1];

		std::string classIconPath = "textures/editor-icons/" + std::string(s_EntityComponentNames[(size_t)primaryComponent.first]) + ".png";
		Texture& tex = texManager->GetTextureResource(texManager->LoadTextureFromPath(classIconPath));

		if (tex.Status == Texture::LoadStatus::Failed && tex.ImagePath.find("fallback") == std::string::npos)
		{
			const std::string& fallbackPath = "texture/editor-icons/fallback.png";
			Texture& fallback = texManager->GetTextureResource(texManager->LoadTextureFromPath(fallbackPath));
			texManager->Assign(fallback, classIconPath);
			tex = fallback;
		}

		ImGui::Image(
			tex.GpuId,
			ImVec2(16.f, 16.f)
		);
		ImGui::SameLine();

		if (!didVisitCurSelection && Selections.size() > 0)
		{
			std::vector<GameObject*> descs = object->GetDescendants();

			for (GameObject* desc : descs)
				if (isInSelections(desc))
				{
					ImGui::SetNextItemOpen(true);
					break;
				}
		}

		bool open = ImGui::TreeNodeEx(&object->ObjectId, flags, "%s", object->Name.c_str());

		VisibleTreeWip.push_back(object->ObjectId);

		if (ImGui::IsItemClicked())
			nodeClicked = object;

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)
			&& ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
			)
		{
			ImGui::OpenPopup(1979);

			if (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift || !isInSelections(object))
				// select/add to selections
				onTreeItemClicked(object);

			ContextActionsForSelection = getPossibleActionsForSelections();
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SameLine();

			ImGuiStyle style = ImGui::GetStyle();

			ImVec2 defLabelSize = ImGui::CalcTextSize("+", NULL, true);
			ImVec2 defaultSize{ defLabelSize.x + style.FramePadding.x * 2.f, defLabelSize.y + style.FramePadding.y * 2.f };

			// make it the same size on both axes
			ImGui::Button("+", ImVec2(defaultSize.y, defaultSize.y));

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

				for (size_t i = 0; i < (size_t)EntityComponent::__count; i++)
					if (ImGui::MenuItem(s_EntityComponentNames[i].data()))
					{
						GameObject* newObject = GameObject::Create(s_EntityComponentNames[i]);
						newObject->SetParent(ObjectInsertionTarget);
						Selections = { newObject->ObjectId };

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
		onTreeItemClicked(nodeClicked);
}

void InlayEditor::UpdateAndRender(double DeltaTime)
{
	ZoneScopedC(tracy::Color::DarkSeaGreen);
	
	ErrorTooltipTimeRemaining -= DeltaTime;

	if (ErrorTooltipTimeRemaining > 0.f)
		ImGui::SetTooltip("%s", ErrorTooltipMessage.c_str());

	renderTextEditor();
	renderShaderPipelinesEditor();
	renderMaterialEditor();

	if (!ImGui::Begin("Editor"))
	{
		ImGui::End();
		return;
	}

	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(227, 227, 227, 255));
	ImGui::Text("Compiled: %s", __DATE__);
	ImGui::PopStyleColor();

	ImVec2 hrchChildWinSzOverride{};

	if (Selections.size() > 0)
		hrchChildWinSzOverride = ImGui::GetContentRegionAvail() * ImVec2(1.f, .4f);

	ImGui::BeginChild("HierarchyChildWindow", hrchChildWinSzOverride, ImGuiChildFlags_Border);

	VisibleTreeWip.clear();
	recursiveIterateTree(GameObject::GetObjectById(GameObject::s_DataModel));
	
	VisibleTree = VisibleTreeWip;

	std::vector<std::unique_ptr<GameObjectRef>> refs;

	for (uint32_t id : Selections)
		refs.push_back(std::make_unique<GameObjectRef>(GameObject::GetObjectById(id)));

	if (ImGui::BeginPopupEx(1979, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::SeparatorText("Actions");

		for (const std::string_view& Action : ContextActionsForSelection)
			if (Action[0] == '$')
				ImGui::SeparatorText(Action.substr(1).data());

			else if (ImGui::MenuItem(Action.data()))
				ActionHandlers[Action]();

		ImGui::EndPopup();
	}

	ImGui::EndChild();

	if (Selections.size() > 0)
	{
		uint32_t idSum = 0;
		for (uint32_t id : Selections)
			idSum += id;

		ImGui::PushID(idSum);

		ImGui::BeginChild("PropertiesEditor", ImVec2(), ImGuiChildFlags_Border);

		std::string sepStr = "Properties of " + std::to_string(Selections.size()) + " objects";
		if (Selections.size() == 1)
		{
			GameObject* target = GameObject::GetObjectById(Selections[0]);

			sepStr = std::vformat(
				"Properties of {}",
				std::make_format_args(target->Name)
			);
		}

		ImGui::SeparatorText(sepStr.c_str());

		std::unordered_map<std::string_view, std::pair<Reflection::Property, Reflection::GenericValue>> props;
		std::unordered_map<std::string_view, bool> conflictingProps;

		for (uint32_t selId : Selections)
		{
			GameObject* sel = GameObject::GetObjectById(selId);

			if (!sel)
			{
				// 25/01/2025 i think the iterator goes boom if i do this
				Selections.erase(std::find(Selections.begin(), Selections.end(), selId));
				break;
			}

			for (const auto& prop : sel->GetProperties())
			{
				const std::string_view& pname = prop.first;
				const auto& it = props.find(pname);
				Reflection::GenericValue myVal = sel->GetPropertyValue(pname);

				props.insert(std::pair(pname, std::pair(prop.second, myVal)));

				if (it != props.end())
				{
					const auto& prevVal = it->second;
					if (prevVal.second != myVal)
						conflictingProps.insert(std::pair(prop.first, true));
					else
						conflictingProps.insert(std::pair(prop.first, false));
				}
			}
		}

		for (const auto& propIt : props)
		{
			const std::pair<Reflection::Property, Reflection::GenericValue> propItem = propIt.second;

			const std::string_view& propName = propIt.first;
			const Reflection::Property& propDesc = propItem.first;
			const Reflection::GenericValue& curVal = propItem.second;

			const char* propNameCStr = propName.data();

			bool doConflict = conflictingProps[propName];

			if (!propDesc.Set)
			{
				// no setter (read-only property, such as Class or ObjectId)
				// 07/07/2024

				if (doConflict)
					ImGui::Text("%s: <DIFFERENT>", propNameCStr);
				else
				{
					std::string curValStr = curVal.ToString();

					if (propName == "Class")
					{
						static TextureManager* texManager = TextureManager::Get();
						std::string classIconPath = "textures/editor-icons/" + curValStr + ".png";

						ImGui::Text("Class: ");
						ImGui::SameLine();

						ImGui::Image(
							texManager->GetTextureResource(texManager->LoadTextureFromPath(classIconPath)).GpuId,
							ImVec2(16, 16)
						);
						ImGui::SameLine();

						ImGui::TextUnformatted(curValStr.c_str());
					}
					else
						if (curVal.Type == Reflection::ValueType::Matrix)
						{
							ImGui::Text("%s: ", propNameCStr);

							ImGui::Indent();

							curValStr.insert(curValStr.begin() + curValStr.find_first_of("Ang"), '\n');
							ImGui::TextUnformatted(curValStr.c_str());

							ImGui::Unindent();
						}
						else
							ImGui::Text("%s: %s", propNameCStr, curValStr.c_str());
				}

				continue;
			}

			Reflection::GenericValue newVal = curVal;
			bool canChangeValue = true;
			bool valueWasEditedManual = false;

			switch (propDesc.Type)
			{

			case Reflection::ValueType::String:
			{
				std::string_view str = curVal.AsStringView();

				const size_t INPUT_TEXT_BUFFER_ADDITIONAL = 64;

				size_t allocSize = str.size() + INPUT_TEXT_BUFFER_ADDITIONAL;

				char* buf = BufferInitialize(
					allocSize,
					"<Initial Value 29/09/2024 Hey guys How we doing today>"
				);

				if (!doConflict)
					memcpy(buf, str.data(), str.size());
				else
					CopyStringToBuffer(buf, allocSize);

				buf[str.size()] = 0;

				ImGui::InputText(propNameCStr, buf, allocSize);
				newVal = buf;

				Memory::Free(buf);

				break;
			}

			case Reflection::ValueType::Boolean:
			{
				bool b = !doConflict ? newVal.AsBoolean() : false;

				ImGui::Checkbox(propNameCStr, &b);

				if (doConflict)
					ImGui::SetItemTooltip("Checking this box will set this member as TRUE for all selected objects");

				newVal = b;

				break;
			}

			case Reflection::ValueType::Double:
			{
				if (!doConflict)
				{
					double d = newVal.AsDouble();

					ImGui::InputDouble(propNameCStr, &d);
					newVal = d;
				}
				else
				{
					char buf[] = { '<', 'C', 'O', 'N', 'F', 'L', 'I', 'C', 'T', '>' , 0 };

					if (ImGui::InputText(propNameCStr, buf, sizeof(buf)))
						newVal = atof(buf);
				}

				break;
			}

			case Reflection::ValueType::Integer:
			{
				if (!doConflict)
				{
					// TODO BIG BAD HACK HACK HACK
					// stoobid Dear ImGui :'(
					// only allows 32-bit integer input
					// 01/09/2024
					int32_t valAs32Bit = static_cast<int32_t>(curVal.AsInteger());

					ImGui::InputInt(propNameCStr, &valAs32Bit);

					newVal = valAs32Bit;
				}
				else
				{
					char buf[] = { '<', 'C', 'O', 'N', 'F', 'L', 'I', 'C', 'T', '>' , 0 };

					if (ImGui::InputText(propNameCStr, buf, sizeof(buf)))
						newVal = (int64_t)atoi(buf);
				}

				break;
			}

			case Reflection::ValueType::GameObject:
			{
				if (!doConflict)
				{
					GameObject* referenced = GameObject::FromGenericValue(curVal);
					std::string str = "";

					if (referenced)
						str = referenced->Name;

					ImGui::InputText(propNameCStr, &str);
					ImGui::SetItemTooltip("CTRL+Click to select referenced GameObject 03/12/2024");

					if (ImGui::IsItemClicked())
						if (ImGui::GetIO().KeyCtrl)
						{
							if (referenced)
							{
								uint32_t targetId = static_cast<uint32_t>(curVal.AsInteger());
								Selections = { targetId };
							}
							else
								Selections.clear();
						}
						else
						{
							IsPickingObject = true;
							PickerTargets = Selections;
							PickerTargetPropName = propName;

							Selections.clear();
						}

					newVal = curVal;
				}
				else
				{
					char buf[] = { '<', 'C', 'O', 'N', 'F', 'L', 'I', 'C', 'T', '>' , 0 };

					if (ImGui::InputText(propNameCStr, buf, sizeof(buf)))
					{
						IsPickingObject = true;
						PickerTargets = Selections;
						PickerTargetPropName = propName;

						Selections.clear();

						canChangeValue = false;
					}
				}

				break;
			}

			case Reflection::ValueType::Color:
			{
				Color col = !doConflict ? curVal : Color(1.f, 1.f, 0.f);

				float entry[3] = { col.R, col.G, col.B };

				ImGui::ColorEdit3(propNameCStr, entry, ImGuiColorEditFlags_None);

				col.R = entry[0];
				col.G = entry[1];
				col.B = entry[2];

				newVal = col.ToGenericValue();

				break;
			}

			case Reflection::ValueType::Vector3:
			{
				if (!doConflict)
				{
					Vector3 vec = curVal;

					float entry[3] =
					{
						static_cast<float>(vec.X),
						static_cast<float>(vec.Y),
						static_cast<float>(vec.Z)
					};

					ImGui::InputFloat3(propNameCStr, entry);

					vec.X = entry[0];
					vec.Y = entry[1];
					vec.Z = entry[2];

					newVal = vec.ToGenericValue();
				}
				else
				{
					char buf[] = { '<', 'C', 'O', 'N', 'F', 'L', 'I', 'C', 'T', '>' , 0 };

					if (ImGui::InputText(propNameCStr, buf, sizeof(buf)))
						newVal = Vector3::zero.ToGenericValue();
				}

				break;
			}

			case Reflection::ValueType::Matrix:
			{
				if (!doConflict)
				{
					glm::mat4 mat = curVal.AsMatrix();

					ImGui::Text("%s:", propNameCStr);

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
					valueWasEditedManual = ImGui::IsItemEdited();

					ImGui::InputFloat3("Rotation", rotdegs);

					ImGui::Unindent();

					mat = glm::mat4(1.f);

					mat[3][0] = pos[0];
					mat[3][1] = pos[1];
					mat[3][2] = pos[2];

					mat *= glm::eulerAngleXYZ(glm::radians(rotdegs[0]), glm::radians(rotdegs[1]), glm::radians(rotdegs[2]));

					newVal = mat;
				}
				else
				{
					char buf[] = { '<', 'C', 'O', 'N', 'F', 'L', 'I', 'C', 'T', '>' , 0 };

					if (ImGui::InputText(propNameCStr, buf, sizeof(buf)))
						newVal = glm::mat4(1.f);
				}

				break;
			}

			default:
			{
				int typeId = static_cast<int>(curVal.Type);
				const std::string_view& typeName = Reflection::TypeAsString(curVal.Type);

				ImGui::TextUnformatted(std::vformat(
					"{}: <Display of ID:{} ('{}') types not unavailable>",
					std::make_format_args(propName, typeId, typeName)
				).c_str());

				break;
			}

			}

			if ((ImGui::IsItemEdited() || valueWasEditedManual) && canChangeValue)
			{
				try
				{
					for (uint32_t selId : Selections)
						GameObject::GetObjectById(selId)->SetPropertyValue(propName, newVal);
				}
				catch (std::string err)
				{
					ErrorTooltipMessage = err;
					ErrorTooltipTimeRemaining = 5.f;
				}
			}
		}

		ImGui::EndChild();
		
		ImGui::PopID();
	}

	ImGui::End();
}
