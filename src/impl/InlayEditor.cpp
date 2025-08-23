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
#include "asset/SceneFormat.hpp"
#include "asset/MeshProvider.hpp"

#include "component/Camera.hpp"
#include "component/Script.hpp"

#include "Utilities.hpp"
#include "UserInput.hpp"
#include "Memory.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

bool InlayEditor::DidInitialize = false;

constexpr uint32_t OBJECT_NEW_CLASSNAME_BUFSIZE = 16;
constexpr uint32_t MATERIAL_NEW_NAME_BUFSIZE = 64;
constexpr uint32_t MATERIAL_TEXTUREPATH_BUFSIZE = 128;
constexpr char MATERIAL_NEW_NAME_DEFAULT[] = "newmaterial";

static bool TextEditorEnabled = false;
static char* TextEditorEntryBuffer = nullptr;
static size_t TextEditorEntryBufferCapacity = 0;
static std::fstream* TextEditorFileStream = nullptr;
static std::set<std::string> TextEditorRecentFiles;
static std::vector<std::string> TextEdOpenFiles;
static size_t TextEdCurrentFileTab = 0;

static nlohmann::json DefaultNewMaterial = 
{
	{ "ColorMap", "textures/materials/plastic.png" },
	{ "specExponent", 32.f },
	{ "specMultiply", 0.5f }
};

static nlohmann::json ObjectDocCommentsJson;

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
			glm::vec3(1.f),
			0u,
			glm::vec3(1.f),
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
			glm::vec3{ 0.57f, 0.57f, 0.57f },
			glm::vec3{ 1.f, 1.f, 1.f }
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

static void setErrorMessage(std::string errm)
{
	ErrorTooltipMessage = errm;
	ErrorTooltipTimeRemaining = 2.5f;
	Log::Error(errm);
}

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

	nlohmann::json docComments;

	try
	{
		bool readSuccess = true;
		std::string contents = FileRW::ReadFile("./gh-assets/wiki/doc-comments.json", &readSuccess);

		if (readSuccess)
			docComments = nlohmann::json::parse(contents);
		else
			Log::Warning("Documentation tooltips will not be available (`doc-comments.json` could not be read)");
	}
	catch (const nlohmann::json::parse_error& Err)
	{
		Log::ErrorF("Parse error reading doc comments for Editor: {}", Err.what());
	}

	if (!docComments.is_null())
	{
		ObjectDocCommentsJson = docComments["GameObject"];
		
		if (ObjectDocCommentsJson.is_null())
		{
			Log::Error("`doc-comments.json` malformed");

			ObjectDocCommentsJson["Base"] = {};
			ObjectDocCommentsJson["Components"] = {};
		}
	}

	InlayEditor::DidInitialize = true;
}

void InlayEditor::Shutdown()
{
	MtlPreviewCamera.Invalidate();
}

static bool mtlIterator(void*, int index, const char** outText)
{
	MaterialManager* mtlManager = MaterialManager::Get();

	RenderMaterial& selected = mtlManager->GetLoadedMaterials()[index];

	*outText = selected.Name.c_str();

	return true;
}

static std::string getFileDirectory(const std::string& FilePath)
{
	size_t lastFwdSlash = FilePath.find_last_of("/");
	char* sdlcwd = SDL_GetCurrentDirectory();
	std::string cwd = sdlcwd;
	free(sdlcwd);

	if (lastFwdSlash == std::string::npos)
#ifdef _WIN32
		return cwd + "resources\\";
#else
		return cwd + "resources/";
#endif
	else
	{
		std::string fileDir = FilePath.substr(0, lastFwdSlash + 1);

		if (fileDir.find("resources/") == std::string::npos)
			fileDir = "resources/" + fileDir;

		return cwd + fileDir + "/";
	}
}

#define EDCHECKEXPR(expr) { if (!(expr)) { setErrorMessage(#expr " failed"); } }

static void textEditorSaveFile()
{
	if (!TextEditorEntryBuffer)
	{
		setErrorMessage("Text Editor tried to save file, but had no text buffer");
		return;
	}

	std::string contents = TextEditorEntryBuffer;

	// always flush the buffer. if a new file is created (i.e. "<NEW>"), and the
	// `contents.empty` early-out triggers as the user opens another file, the new file
	// will be overwritten with the empty contents
	// 12/02/2024
	Memory::Free(TextEditorEntryBuffer);
	TextEditorEntryBuffer = nullptr;

	std::string textEditorFile = TextEdOpenFiles[TextEdCurrentFileTab];

	if (textEditorFile == "" || textEditorFile == "<NEW>")
	{
		if (contents.empty())
			return;

		static uint32_t ErrCount = 0;
		ErrCount++;

		textEditorFile = "texteditor_default_" + std::to_string(ErrCount) + ".txt";
		setErrorMessage("Text Editor tried to save a file with no path. Will be saved to " + textEditorFile);
	}
	else
		TextEditorRecentFiles.insert(textEditorFile);

	if (TextEditorFileStream && TextEditorFileStream->is_open())
	{
		TextEditorFileStream->close();
		delete TextEditorFileStream;
		TextEditorFileStream = nullptr;
	}

	std::string realSaveLoc = FileRW::MakePathCwdRelative(textEditorFile);

	Log::InfoF("Saving file to {}", realSaveLoc);
	EDCHECKEXPR(FileRW::WriteFile(realSaveLoc, contents));
}

static void invokeTextEditor(const std::string& File)
{
	if (TextEditorEntryBuffer)
		textEditorSaveFile();

	TextEditorEnabled = true;

	for (size_t i = 0; i < TextEdOpenFiles.size(); i++)
	{
		if (TextEdOpenFiles[i] == File)
		{
			TextEdCurrentFileTab = i;
			return;
		}
	}

	TextEdOpenFiles.push_back(File);
	TextEdCurrentFileTab = TextEdOpenFiles.size() - 1;
}

static void renderTextEditor()
{
	ZoneScopedC(tracy::Color::DarkSeaGreen);

	if (!TextEditorEnabled)
	{
		if (TextEditorEntryBuffer)
			textEditorSaveFile();
		return;
	}

	ImGui::Begin("Text Editor", 0, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar);

	ImGui::BeginMenuBar();

	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::MenuItem("New"))
		{
			textEditorSaveFile();
			TextEdOpenFiles.push_back("<NEW>");
			TextEdCurrentFileTab = TextEdOpenFiles.size() - 1;

			TextEditorEntryBuffer = BufferInitialize(512);
			TextEditorEntryBufferCapacity = 512;
		}

		if (ImGui::MenuItem("Open"))
		{
			TextEditorRecentFiles.insert(TextEdOpenFiles[TextEdCurrentFileTab]);
			std::string curDir = getFileDirectory(TextEdOpenFiles[TextEdCurrentFileTab]);

			SDL_ShowOpenFileDialog(
				[](void*, const char* const* FileList, int)
				{
					if (!FileList)
						RAISE_RT(SDL_GetError());
					if (!FileList[0])
						return;

					textEditorSaveFile();
					TextEditorRecentFiles.insert(TextEdOpenFiles[TextEdCurrentFileTab]);

					TextEdOpenFiles.push_back(FileRW::MakePathCwdRelative(FileList[0]));
					TextEdCurrentFileTab = TextEdOpenFiles.size() - 1;
				},
				nullptr,
				SDL_GL_GetCurrentWindow(),
				nullptr,
				0,
				curDir.c_str(),
				false
			);
		}

		std::string textEditorFile = TextEdOpenFiles[TextEdCurrentFileTab];

		if (ImGui::MenuItem("Save", NULL, nullptr, textEditorFile != "" && textEditorFile != "<NEW>"))
			textEditorSaveFile();

		if (ImGui::MenuItem("Save As"))
		{
			std::string curDir = getFileDirectory(textEditorFile);

			SDL_ShowSaveFileDialog(
				[](void*, const char* const* FileList, int)
				{
					if (!FileList)
						RAISE_RT(SDL_GetError());
					if (!FileList[0])
						return;

					TextEdOpenFiles.push_back(FileRW::MakePathCwdRelative(FileList[0]));
					TextEdCurrentFileTab = TextEdOpenFiles.size() - 1;
					textEditorSaveFile();

					TextEditorRecentFiles.insert(TextEdOpenFiles[TextEdCurrentFileTab]);
				},
				nullptr,
				SDL_GL_GetCurrentWindow(),
				nullptr,
				0,
				curDir.c_str()
			);
		}

		if (ImGui::BeginMenu("Recent", TextEditorRecentFiles.size() > 0) || ImGui::IsItemHovered())
		{
			std::set<std::string>::reverse_iterator rit;
		
			for (rit = TextEditorRecentFiles.rbegin(); rit != TextEditorRecentFiles.rend(); rit++)
			{
				std::string label = rit->data();
				if (label.find("resources/") == 0)
					label = label.substr(strlen("resources/"), label.size() - strlen("resources/"));

				if (ImGui::MenuItem(label.c_str()))
				{
					textEditorSaveFile();
					TextEdOpenFiles.push_back(rit->data());
					TextEdCurrentFileTab = TextEdOpenFiles.size() - 1;
				}
			}

			ImGui::EndMenu();
		}

		if (ImGui::MenuItem("Close"))
		{
			TextEditorEnabled = false;
			textEditorSaveFile();
		}

		ImGui::EndMenu();
	}

	ImGui::Separator();

	static size_t NextButtonHighlight = UINT64_MAX;
	static size_t HoveredId = UINT64_MAX;
	size_t wipNextButtonHighlight = UINT64_MAX;
	size_t wipHoveredId = UINT64_MAX;

	for (size_t i = 0; i < TextEdOpenFiles.size(); i++)
	{
		ImGui::PushID(i);

		std::string label = TextEdOpenFiles[i];
		if (label.find("resources/") == 0)
			label = label.substr(strlen("resources/"), label.size() - strlen("resources/"));

		if (i == TextEdCurrentFileTab && HoveredId == i && label.size() > 5)
		{
			label = label.substr(0, label.size() - 3);
			label.append("... ");
		}

		if (ImGui::MenuItemEx(
			label.c_str(),
			nullptr,
			nullptr,
			(i == TextEdCurrentFileTab) && (HoveredId != i),
			true
		) && TextEdCurrentFileTab != i)
		{
			textEditorSaveFile();
			TextEdCurrentFileTab = i;
		}

		bool hovered = ImGui::IsItemHovered();
		if (hovered)
			wipHoveredId = i;

		if (i == TextEdCurrentFileTab && hovered)
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 20.f);

			if (HoveredId == i)
			{
				if (i == NextButtonHighlight)
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.f, 0.f, 1.f, 1.f));
				else
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.f, 0.f, 0.f, 1.f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 0.f, 1.f, 1.f));
			}

			bool closeTab = ImGui::Button("X", ImVec2(16.f, 18.f)) || ImGui::IsItemClicked();

			if (ImGui::IsItemHovered())
			{
				wipNextButtonHighlight = i;
				wipHoveredId = i;
			}

			if (HoveredId == i)
				ImGui::PopStyleColor(2);

			if (closeTab || ImGui::IsItemClicked())
			{
				textEditorSaveFile();
				TextEdOpenFiles.erase(TextEdOpenFiles.begin() + i);
			}
		}

		ImGui::PopID();
	}
	NextButtonHighlight = wipNextButtonHighlight;
	HoveredId = wipHoveredId;

	ImGui::EndMenuBar();

	if (!TextEditorEntryBuffer)
	{
		if (TextEditorFileStream)
		{
			TextEditorFileStream->close();
			delete TextEditorFileStream;
		}

		if (TextEdCurrentFileTab >= TextEdOpenFiles.size())
		{
			if (TextEdOpenFiles.size() > 0)
				TextEdCurrentFileTab = TextEdOpenFiles.size() - 1;
			else
			{
				TextEdOpenFiles.push_back("<NEW>");
				TextEdCurrentFileTab = 0;
			}
		}

		TextEditorFileStream = new std::fstream(FileRW::MakePathCwdRelative(TextEdOpenFiles[TextEdCurrentFileTab]));
		
		std::string scriptContents = "";

		if (!(*TextEditorFileStream) || !TextEditorFileStream->is_open())
		{
			TextEditorEntryBuffer = BufferInitialize(512);
			TextEditorEntryBufferCapacity = 512;

			if (TextEdOpenFiles[TextEdCurrentFileTab] != "<NEW>")
			{
				setErrorMessage(std::format(
					"File '{}' couldn't be opened",
					TextEdOpenFiles[TextEdCurrentFileTab]
				));
				TextEdOpenFiles.push_back("<NEW>");
				TextEdCurrentFileTab = TextEdOpenFiles.size() - 1;

				copyStringToBuffer(
					TextEditorEntryBuffer,
					"Couldn't read file:\n" + TextEdOpenFiles[TextEdCurrentFileTab],
					512
				);
			}
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

	ImGui::Combo("Variable Type", &NewTypeId, "Boolean\0Integer\0Float\0");

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

	std::vector<std::string> uniformsArray;
	uniformsArray.reserve(Uniforms.size());

	for (auto& it : Uniforms)
		uniformsArray.push_back(it.first);

	ImGui::ListBox(
		"Uniforms",
		Selection,
		[](void* ud, int index)
		{
			std::vector<std::string>* uniforms = (std::vector<std::string>*)ud;
			return uniforms->at(index).c_str();
		},
		&uniformsArray,
		static_cast<int>(Uniforms.size())
	);

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
			ImGui::InputDouble("Value", &value.Val.Double);

			newValue = value;
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
					RAISE_RT(SDL_GetError());
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
					setErrorMessage("Selection must be within the Project's `resources/` directory!");
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
	{
		if (ImGui::Button("Add"))
		{
			addTextureDialog = true;
			memcpy(CurrentPath, (const char*)("textures/"), 9); // start the file dialog in the textures directory
		}
		else
		{
			ImGui::PopID();
			return;
		}
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
						RAISE_RT(SDL_GetError());
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
						setErrorMessage("Selection must be within the Project's `resources/` directory!");
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

		ImGui::Text("Resolution: %ix%i", tx.Width, tx.Height);
		ImGui::Text("# Color channels: %i", tx.NumColorChannels);

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
		EDCHECKEXPR(FileRW::WriteFile(
			"materials/" + std::string(MtlCreateNameBuf) + ".mtl",
			DefaultNewMaterial.dump(2)
		));

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
	ImGui::Combo("Polygon Mode", &curPolyMode, "Fill\0Line\0Point\0");
	curItem.PolygonMode = static_cast<RenderMaterial::MaterialPolygonMode>(std::clamp(curPolyMode, 0, 2));

	mtlEditorTexture("Color Map: ", &curItem.ColorMap, MtlDiffuseBuf, false);
	mtlEditorTexture("Metallic-Roughness Map: ", &curItem.MetallicRoughnessMap, MtlSpecBuf, false);
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

enum class ContextMenuAction : uint8_t
{
	Duplicate = 0,
	Delete,
	SaveToFile,
	LoadFromFile,
	EditScript,
	ReloadScript,

	__sectionSeparator
};

static std::vector<ContextMenuAction> ContextActionsForSelection;

static const char* ContextMenuActionStrings[] =
{
	"Duplicate",
	"Delete",
	"Save to File",
	"Insert from File",
	"Edit",
	"Reload"
};

static std::function<void(void)> ContextMenuActionHandlers[] =
{
	[]()
	{
		std::vector<uint32_t> newSelections;
		newSelections.reserve(Selections.size());
		
		for (uint32_t selId : Selections)
			if (GameObject* g = GameObject::GetObjectById(selId))
				newSelections.push_back(g->Duplicate()->ObjectId);
	
		Selections = newSelections;
	},

	[]()
	{
		for (uint32_t selId : Selections)
			if (GameObject* g = GameObject::GetObjectById(selId))
				g->Destroy();
				
		Selections.clear();
	},

	[]()
	{
		std::vector<GameObject*> sels;
		std::string sceneName = "";

		for (uint32_t s : Selections)
			if (GameObject* g = GameObject::GetObjectById(s))
			{
				sels.push_back(g);
				sceneName += g->Name + "_";
			}

		sceneName = sceneName.substr(0, sceneName.size() - 1);
		std::string* ser = new std::string(SceneFormat::Serialize(sels, "SaveToFileAction_" + sceneName));
		
		SDL_ShowSaveFileDialog(
			[](void* contentsptr, const char* const* FileList, int)
			{
				std::string* contentsptrstr = (std::string*)contentsptr;
				std::string contents = *contentsptrstr;
				delete contentsptrstr;

				if (!FileList)
					RAISE_RT(SDL_GetError());
				if (!FileList[0])
					return;
					
				EDCHECKEXPR(FileRW::WriteFile(FileList[0], contents));
			},
			(void*)ser,
			SDL_GL_GetCurrentWindow(),
			nullptr,
			0,
			getFileDirectory("resources/scenes/dummy.file").c_str()
		);
	},

	[]()
	{
		SDL_ShowOpenFileDialog(
			[](void*, const char* const* FileList, int)
			{
				if (!FileList)
					RAISE_RT(SDL_GetError());
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
					setErrorMessage("Selection must be within the Project's `resources/` directory!");
				}
				else
				{
					bool readSuccess = false;
					std::string contents = FileRW::ReadFile(fullpath, &readSuccess);

					if (!readSuccess)
					{
						setErrorMessage("Couldn't read file " + fullpath);
						return;
					}

					std::vector<GameObjectRef> roots = SceneFormat::Deserialize(contents, &readSuccess);

					if (!readSuccess)
					{
						setErrorMessage(SceneFormat::GetLastErrorString());
						return;
					}

					assert(GameObject::GetObjectById(Selections[0]));

					for (GameObjectRef r : roots)
						r->SetParent(GameObject::GetObjectById(Selections[0]));
				}
			},
			nullptr,
			SDL_GL_GetCurrentWindow(),
			nullptr,
			0,
			getFileDirectory("resources/scenes/dummy.file").c_str(),
			false
		);
	},

	[]()
	{
		for (uint32_t selId : Selections)
			if (EcScript* s = GameObject::GetObjectById(selId)->GetComponent<EcScript>())
				invokeTextEditor(s->SourceFile);
	},

	[]()
	{
		for (uint32_t selId : Selections)
			if (EcScript* s = GameObject::GetObjectById(selId)->GetComponent<EcScript>())
				s->Reload();
	}
};

static bool isInSelections(GameObject* g)
{
	return std::find(Selections.begin(), Selections.end(), g->ObjectId) != Selections.end();
}

static std::vector<ContextMenuAction> getPossibleActionsForSelections()
{
	std::vector<ContextMenuAction> actions;
	actions.reserve(4);

	bool gotScript = false;

	for (uint32_t selId : Selections)
	{
		GameObject* g = GameObject::GetObjectById(selId);

		if (!gotScript)
			if (g->GetComponent<EcScript>())
			{
				actions.push_back(ContextMenuAction::EditScript);
				actions.push_back(ContextMenuAction::ReloadScript);
				actions.push_back(ContextMenuAction::__sectionSeparator);

				break;
			}
	}

	actions.push_back(ContextMenuAction::Duplicate);
	actions.push_back(ContextMenuAction::Delete);
	actions.push_back(ContextMenuAction::SaveToFile);
	actions.push_back(ContextMenuAction::LoadFromFile);

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
		catch (const std::runtime_error& Error)
		{
			setErrorMessage(Error.what());
		}

		// restore prev selections
		Selections = PickerTargets;

		IsPickingObject = false;
		PickerTargets.clear();
		PickerTargetPropName.clear();

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

static Texture getIconForComponent(EntityComponent Ec)
{
	TextureManager* texManager = TextureManager::Get();

	std::string classIconPath = "textures/editor-icons/" + std::string(s_EntityComponentNames[(size_t)Ec]) + ".png";
	Texture tex = texManager->GetTextureResource(texManager->LoadTextureFromPath(classIconPath, true, false));

	if (tex.Status == Texture::LoadStatus::Failed && tex.ImagePath.find("fallback") == std::string::npos)
	{
		const std::string& fallbackPath = "textures/editor-icons/fallback.png";
		Texture& fallback = texManager->GetTextureResource(texManager->LoadTextureFromPath(fallbackPath, true, false));
		texManager->Assign(fallback, classIconPath);
		tex = fallback;
	}

	return tex;
}

static std::string getDescriptionForComponent(EntityComponent Ec)
{
	static std::vector<std::string> Descriptions;
	size_t index = (size_t)Ec;

	if (Descriptions.size() < index + 1)
		Descriptions.resize(index + 1);

	if (Descriptions[index].size() == 0)
	{
		std::string desc = "";

		if (const auto& it = ObjectDocCommentsJson["Components"].find(s_EntityComponentNames[index]);
			(it != ObjectDocCommentsJson["Components"].end() && it.value().is_object())
		)
		{
			const auto& descJson = it.value()["Description"];
		
			if (descJson.is_string())
				desc = descJson;
		
			else if (descJson.is_array())
			{
				for (size_t i = 0; i < descJson.size(); i++)
					desc += std::string(descJson[i]) + "\n";
				
				desc = desc.substr(0, desc.size() - 1);
			}
		}

		Descriptions[index] = desc;
	}

	return Descriptions[index];
}

static void recursiveIterateTree(GameObject* current, bool didVisitCurSelection = false)
{
	ZoneScopedC(tracy::Color::DarkSeaGreen);

	if (IsPickingObject)
	{
		ErrorTooltipMessage = "Pick Object";
		ErrorTooltipTimeRemaining = 0.1f;

		if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
		{
			Selections = PickerTargets;

			IsPickingObject = false;
			PickerTargets.clear();
			PickerTargetPropName.clear();
		}
	}

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
			RAISE_RT("stoopid compiler is giving me a warning for something that will probably not happen");

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
			| ImGuiTreeNodeFlags_AllowItemOverlap
			| ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_DrawLinesFull;

		// make the insert button have better contrast
		if (isInSelections(object) && object != InsertObjectButtonHoveredOver)
			flags |= ImGuiTreeNodeFlags_Selected;

		if (object->GetChildren().empty())
			flags |= ImGuiTreeNodeFlags_Leaf;

		ImGui::AlignTextToFramePadding();

		std::pair<EntityComponent, uint32_t> primaryComponent = object->GetComponents()[0];
		if (primaryComponent.first == EntityComponent::Transform && object->GetComponents().size() > 1)
			primaryComponent = object->GetComponents()[1];
		
		Texture tex = getIconForComponent(primaryComponent.first);

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

		const ImGuiStyle& style = ImGui::GetStyle();

		bool openInserter = false;
		bool isHovered = false;

		bool open = ImGui::TreeNodeEx(&object->ObjectId, flags, "%s", "");
		if (ImGui::IsItemClicked())
			nodeClicked = object;
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
			isHovered = true;
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && isHovered)
			openInserter = true;

		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() - style.IndentSpacing * 0.6f + 1.5f);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.f); // not the faintest idea

		ImGui::Image(
			tex.GpuId,
			ImVec2(16.f, 16.f)
		);
		if (ImGui::IsItemClicked())
			nodeClicked = object;
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
			isHovered = true;
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && isHovered)
			openInserter = true;
		
		ImGui::SameLine();
		ImGui::TextUnformatted(object->Name.c_str());

		if (ImGui::IsItemClicked())
			nodeClicked = object;
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
			isHovered = true;
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && isHovered)
			openInserter = true;

		VisibleTreeWip.push_back(object->ObjectId);

		if (openInserter || (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && isHovered))
		{
			ImGui::OpenPopup(1979); // the mimic!!

			if (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift || !isInSelections(object))
				// select/add to selections
				onTreeItemClicked(object);

			ContextActionsForSelection = getPossibleActionsForSelections();
		}

		if (isHovered || ImGui::IsItemHovered())
		{
			ImGui::SameLine();

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

				// skip `<NONE>`
				for (size_t i = 1; i < (size_t)EntityComponent::__count; i++)
				{
					std::string_view name = s_EntityComponentNames[i];
					std::string tooltip;

					bool clicked = ImGui::MenuItem(name.data());

					if (ImGui::IsItemHovered())
						tooltip = getDescriptionForComponent((EntityComponent)i);

					if (tooltip.size() > 1)
						ImGui::SetItemTooltip("%s", tooltip.c_str());

					if (clicked)
					{
						GameObject* newObject = GameObject::Create(s_EntityComponentNames[i]);
						newObject->SetParent(ObjectInsertionTarget);
						Selections = { newObject->ObjectId };

						ObjectInsertionTarget = nullptr;
					}
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

static bool resetConflictedProperty(const char* PropName, char* Buf = nullptr)
{
	char dbuf[2] = { 0 };
	Buf = Buf ? Buf : dbuf;

	bool reset = ImGui::InputText(PropName, Buf, 2);
	ImGui::SetItemTooltip("Start typing here to reset this conflicting property to a default");

	return reset;
}

static void propertyTooltip(const std::string_view& PropName, bool IsObjectProp = false)
{
	static std::unordered_map<std::string, std::string> PropTips;

	if (ImGui::IsItemHovered() && !ImGui::GetIO().WantCaptureKeyboard)
	{
		std::string PropNameStr{ PropName };

		auto pcit = PropTips.find(PropNameStr);

		if (pcit == PropTips.end())
		{
			auto it = ObjectDocCommentsJson["Base"]["Properties"].find(PropNameStr);
			bool found = it != ObjectDocCommentsJson["Base"]["Properties"].end();

			if (!found)
			{
				for (auto cit = ObjectDocCommentsJson["Components"].begin();
					cit != ObjectDocCommentsJson["Components"].end();
					cit++
				)
				{
					it = cit.value()["Properties"].find(PropNameStr);
				
					if (it != cit.value()["Properties"].end())
					{
						found = true;
						break;
					}
				}
			}

			if (found)
			{
				std::string tip = it.value();
				constexpr size_t MaxCharactersBeforeNewline = 75;
				int64_t naturalMaxCharactersPerLine = (size_t)MaxCharactersBeforeNewline;

				if (tip.size() > MaxCharactersBeforeNewline)
				{
					size_t longestWithoutNewline = tip.find('\n');
					int64_t lineOffset = 0;

					while (longestWithoutNewline > (size_t)naturalMaxCharactersPerLine)
					{
						size_t nearest = std::string::npos;
						int64_t nearestDistance = naturalMaxCharactersPerLine / 2;
						int64_t targetSplitPoint = lineOffset + naturalMaxCharactersPerLine;

						for (int64_t i = 0; i < (int64_t)tip.size(); i++)
						{
							char c = tip[i];

							if (c == ' ' && tip.size() - i > (size_t)naturalMaxCharactersPerLine / 4)
							{
								int64_t distance = std::abs(i - targetSplitPoint);

								if (distance < nearestDistance
									// prefer cutting forward
									|| (std::abs(distance - nearestDistance) < 4 && i > (int64_t)nearest)
								)
								{
									nearest = i;
									nearestDistance = distance;
								}
							}
							else if (tip[i + 1] == ' ' &&
								(c == '.' || c == ',' || c == ';' || c == ':' || c == '!' || c == '-' || c == '?' || c == ')')
							)
							{
								int64_t distance = std::abs(i - targetSplitPoint);
								distance -= 4; // DISTANCE CANNOT BE UNSIGNED

								if (distance < nearestDistance)
								{
									nearest = i + 1; // replace the proceeding space instead of removing punctuation
									nearestDistance = distance;
								}
							}
						}

						if (nearest == std::string::npos)
							break;

						tip[nearest] = '\n';
						lineOffset += nearest;
						longestWithoutNewline = std::string_view(tip.begin() + nearest + 1, tip.end()).find('\n');

						// if we end up creating a line that's longer than `MaxCharactersBeforeNewline`,
						// (due to long words etc), align to that instead
						int64_t newNaturalMaxCharactersPerLine = 0;
						for (size_t i = 0; i < nearest; i++)
						{
							newNaturalMaxCharactersPerLine++;

							if (tip[i] == '\n')
								newNaturalMaxCharactersPerLine = 0;
						}

						naturalMaxCharactersPerLine = std::max(naturalMaxCharactersPerLine, newNaturalMaxCharactersPerLine);
					} 
				}

				PropTips[PropNameStr] = tip;
				pcit = PropTips.find(PropNameStr);
			}
		}

		std::string fulltip = IsObjectProp ? "\n(CTRL+Click to select)" : "";
		
		if (pcit != PropTips.end())
			fulltip = pcit->second + fulltip;

		ImGui::SetItemTooltip("%s", fulltip.data());
	}
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

	std::vector<GameObjectRef> refs;

	for (uint32_t id : Selections)
		if (GameObject* g = GameObject::GetObjectById(id))
			refs.emplace_back(g);
		else
			Selections.erase(std::find(Selections.begin(), Selections.end(), id));
	
	if (ImGui::BeginPopupEx(1979, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::SeparatorText("Actions");

		for (ContextMenuAction action : ContextActionsForSelection)
			if (action == ContextMenuAction::__sectionSeparator)
				ImGui::Separator();

			else if (ImGui::MenuItem(ContextMenuActionStrings[(uint8_t)action]))
				ContextMenuActionHandlers[(uint8_t)action]();

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

			sepStr = std::format(
				"Properties of {}",
				target->Name
			);
		}

		ImGui::SeparatorText(sepStr.c_str());

		static bool ShowComponents = false;
		if (ImGui::IsItemClicked())
			ShowComponents = !ShowComponents;

		ImGui::PushID(static_cast<uint32_t>(115111116109));

		std::set<EntityComponent> components;

		for (uint32_t selId : Selections)
		{
			GameObject* sel = GameObject::GetObjectById(selId);

			if (!sel)
				continue;

			for (auto [ec, id] : sel->GetComponents())
				components.insert(ec);
		}

		for (EntityComponent ec : components)
		{
			Texture tex = getIconForComponent(ec);
			ImGui::Image(tex.GpuId, ImVec2(16, 16));
			ImGui::SetItemTooltip(
				"%s\n%s",
				s_EntityComponentNames[(size_t)ec].data(),
				getDescriptionForComponent(ec).c_str()
			);
			ImGui::SameLine();
		}

		ImGui::NewLine();
		ImGui::PopID();

		std::unordered_map<std::string_view, std::pair<const Reflection::Property*, Reflection::GenericValue>> props;
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

				Reflection::GenericValue myVal;

				if (prop.second->Get)
					myVal = sel->GetPropertyValue(pname);
				else
					myVal = "<CANNOT_READ>";
				
				props.insert(std::pair(pname, std::pair(prop.second, myVal)));

				if (it != props.end())
				{
					const auto& cit = conflictingProps.find(prop.first);
					// avoid changing this to false if it was already true
					if (cit != conflictingProps.end())
						if (cit->second)
							continue;

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
			const std::pair<const Reflection::Property*, Reflection::GenericValue> propItem = propIt.second;

			const std::string_view& propName = propIt.first;
			const Reflection::Property* propDesc = propItem.first;
			const Reflection::GenericValue& curVal = propItem.second;

			const char* propNameCStr = propName.data();

			bool doConflict = conflictingProps[propName];

			if (!propDesc->Set)
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

				propertyTooltip(propName);

				continue;
			}

			Reflection::GenericValue newVal = curVal;
			bool canChangeValue = true;
			bool valueWasEditedManual = false;

			switch (curVal.Type)
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
				if (doConflict)
				{
					if (resetConflictedProperty(propNameCStr))
						newVal = false;
				}
				else
					ImGui::Checkbox(propNameCStr, &newVal.Val.Bool);

				break;
			}

			case Reflection::ValueType::Double:
			{
				if (doConflict)
				{
					if (char buf[2] = {0}; resetConflictedProperty(propNameCStr, buf))
						newVal = atof(buf);
				}
				else
					ImGui::InputDouble(propNameCStr, &newVal.Val.Double);

				break;
			}

			case Reflection::ValueType::Integer:
			{
				if (doConflict)
				{
					if (char buf[2] = {0}; resetConflictedProperty(propNameCStr))
						newVal = (int64_t)atoi(buf);
				}
				else
				{
					int32_t i = static_cast<int32_t>(newVal.Val.Int);
					ImGui::InputInt(propNameCStr, &i);
					newVal.Val.Int = i;
				}

				break;
			}

			case Reflection::ValueType::GameObject:
			{
				if (doConflict)
				{
					bool pick = ImGui::Button(" ");
					ImGui::SameLine();
					ImGui::TextUnformatted(propNameCStr);

					if (pick)
					{
						IsPickingObject = true;
						PickerTargets = Selections;
						PickerTargetPropName = propName;

						Selections.clear();

						canChangeValue = false;
					}
				}
				else
				{
					GameObject* referenced = GameObject::FromGenericValue(newVal);
					std::string str = "";

					if (referenced)
						str = referenced->Name;

					ImGui::InputText(propNameCStr, &str);

					if (ImGui::IsItemClicked())
					{
						if (ImGui::GetIO().KeyCtrl)
						{
							if (referenced)
							{
								uint32_t targetId = GameObject::FromGenericValue(newVal)->ObjectId;
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
					}
				}

				break;
			}

			case Reflection::ValueType::Color:
			{
				if (doConflict)
				{
					if (resetConflictedProperty(propNameCStr))
						newVal = Color(0.f, 0.f, 0.f).ToGenericValue();
				}
				else
					ImGui::ColorEdit3(propNameCStr, &newVal.Val.Vec3.x);

				break;
			}

			case Reflection::ValueType::Vector2:
			{
				if (doConflict)
				{
					if (resetConflictedProperty(propNameCStr))
						newVal = glm::vec2(0.f);
				}
				else
				{
					float f = static_cast<float>(newVal.Val.Double);
					ImGui::InputFloat2(propNameCStr, &f);
					newVal.Val.Double = f;
				}

				break;
			}

			case Reflection::ValueType::Vector3:
			{
				if (doConflict)
				{
					if (resetConflictedProperty(propNameCStr))
						newVal = glm::vec3(0.f);
				}
				else
					ImGui::InputFloat3(propNameCStr, &newVal.Val.Vec3.x);

				break;
			}

			case Reflection::ValueType::Matrix:
			{
				if (!doConflict)
				{
					glm::mat4 mat = curVal.AsMatrix();

					ImGui::Text("%s:", propNameCStr);
					propertyTooltip(propName);

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
					propertyTooltip(propName);
					valueWasEditedManual = ImGui::IsItemEdited();

					ImGui::InputFloat3("Rotation", rotdegs);
					propertyTooltip(propName);
					ImGui::Unindent();

					mat = glm::mat4(1.f);

					mat[3][0] = pos[0];
					mat[3][1] = pos[1];
					mat[3][2] = pos[2];

					mat *= glm::eulerAngleXYZ(glm::radians(rotdegs[0]), glm::radians(rotdegs[1]), glm::radians(rotdegs[2]));

					newVal = mat;
				}
				else
					if (resetConflictedProperty(propNameCStr))
						newVal = glm::mat4(1.f);

				break;
			}

			default:
			{
				int typeId = static_cast<int>(curVal.Type);
				std::string typeName = Reflection::TypeAsString(curVal.Type);

				ImGui::Text(
					"%s: <Editing of ID:%i ('%s') types not supported>",
					propName.data(), typeId, typeName.c_str()
				);

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
				catch (const std::runtime_error& Err)
				{
					setErrorMessage(Err.what());
				}
			}
			else
			{
				propertyTooltip(propName, curVal.Type == Reflection::ValueType::GameObject);
			}
		}

		ImGui::EndChild();
		
		ImGui::PopID();
	}

	ImGui::End();
}
