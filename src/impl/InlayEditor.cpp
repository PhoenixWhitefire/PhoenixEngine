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
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_events.h>
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
static ObjectHandle MtlPreviewCamera;
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

static ObjectHandle ExplorerRoot;

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

#include "script/ScriptEngine.hpp"
static void debugBreakHook(lua_State*, lua_Debug*, bool HasError, bool);

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

	ScriptEngine::L::DebugBreak = &debugBreakHook;

	InlayEditor::DidInitialize = true;
}

static const char* mtlIterator(void*, int index)
{
	MaterialManager* mtlManager = MaterialManager::Get();
	RenderMaterial& selected = mtlManager->GetLoadedMaterials()[index];

	return selected.Name.c_str();
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
#define UNSAVEDTAG "<NEW>"
#define SAVINGTAG ">///SAVING...///<"

static bool textEditorAskSaveFileAs(
	const std::string& Contents,
	const std::string& Reason = "Do you want to save the following file?"
)
{
	std::string message = std::format(
		"{}\n\nLength: {} characters\nBody:\n{}{}",
		Reason,
		Contents.size(), Contents.substr(0, std::min((size_t)50ull, Contents.size())),
		Contents.size() > (size_t)50ull ? "\n... (truncated)" : ""
	);
	Log::InfoF("`textEditorAskSaveFileAs` prompting: {}", message);

	SDL_MessageBoxButtonData buttons[2] =
	{
		{
			.buttonID = 0,
			.text = "Yes"
		},
		{
			.buttonID = 1,
			.text = "No"
		}
	};

	SDL_MessageBoxData messageData{};
	messageData.flags = SDL_MESSAGEBOX_WARNING;
	messageData.window = SDL_GL_GetCurrentWindow();
	messageData.title = "Save File?";
	messageData.message = message.c_str();
	messageData.numbuttons = 2;
	messageData.buttons = buttons;

	int chosenOption = 0;
	PHX_ENSURE(SDL_ShowMessageBox(&messageData, &chosenOption));

	if (chosenOption == 1)
		return false;

	static bool SaveOperationFinished = false;
	static bool DidSave = false;
	SaveOperationFinished = false;

	SDL_ShowSaveFileDialog(
		[](void* FileContents, const char* const* FileList, int)
		{
			if (!FileList)
				RAISE_RT(SDL_GetError());

			const char* rawPath = FileList[0];
			if (!rawPath)
			{
				Log::Info("No file path selected in `textEditorAskSaveFileAs`");

				DidSave = false;
				SaveOperationFinished = true;
				return;
			}
			std::string savePath = FileRW::MakePathCwdRelative(rawPath);

			if (TextEdOpenFiles[TextEdCurrentFileTab] == SAVINGTAG)
				TextEdOpenFiles[TextEdCurrentFileTab] = savePath;
			else
				for (size_t i = 0; i < TextEdOpenFiles.size(); i++)
					if (TextEdOpenFiles[TextEdCurrentFileTab] == SAVINGTAG)
						TextEdOpenFiles[TextEdCurrentFileTab] = savePath;

			std::string* contents = (std::string*)FileContents;
			std::string errMessage;
			if (!FileRW::WriteFile(savePath, *contents, &errMessage))
			{
				Log::ErrorF("`textEditorAskSaveFileAs` save failed with {} to {}, re-prompting", savePath, errMessage);
				textEditorAskSaveFileAs(
					*contents,
					std::format("Couldn't save to file '{}' because of error {}\nRetry?", savePath, errMessage)
				);
			}
			else
				TextEditorRecentFiles.insert(savePath);

			delete contents;
			DidSave = true;
			SaveOperationFinished = true;
		},
		new std::string(Contents),
		SDL_GL_GetCurrentWindow(),
		nullptr,
		0,
		nullptr
	);

	while (!SaveOperationFinished)
	{
		SDL_PumpEvents();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	return DidSave;
}

static bool TextEditorFileModified = false;

static void textEditorSaveFile()
{
	if (!TextEditorEntryBuffer)
	{
		setErrorMessage("Text Editor tried to save file, but had no text buffer");
		return;
	}

	std::string contents = TextEditorEntryBuffer;

	// always flush the buffer. if a new file is created (i.e. "<NEW>"/UNSAVEDTAG), and the
	// `contents.empty` early-out triggers as the user opens another file, the new file
	// will be overwritten with the empty contents
	// 12/02/2024
	Memory::Free(TextEditorEntryBuffer);
	TextEditorEntryBuffer = nullptr;

	if (!TextEditorFileModified)
	{
		setErrorMessage("File not modified, not saving");
		return;
	}
	TextEditorFileModified = false;

	std::string textEditorFile = TextEdOpenFiles[TextEdCurrentFileTab];

	if (textEditorFile == SAVINGTAG)
	{
		setErrorMessage("Can't... Shouldn't try and save that... How'd you do this?");
		return;
	}

	if (TextEditorFileStream)
	{
		if (TextEditorFileStream->is_open())
			TextEditorFileStream->close();
		delete TextEditorFileStream;
		TextEditorFileStream = nullptr;
	}

	if (textEditorFile == UNSAVEDTAG)
	{
		if (contents.empty())
			return;

		static uint32_t ErrCount = 0;
		ErrCount++;

		TextEdOpenFiles[TextEdCurrentFileTab] = SAVINGTAG;
		if (!textEditorAskSaveFileAs(contents))
			TextEdOpenFiles[TextEdCurrentFileTab] = UNSAVEDTAG;
	}
	else
	{
		TextEditorRecentFiles.insert(textEditorFile);

		std::string realSaveLoc = FileRW::MakePathCwdRelative(textEditorFile);

		Log::InfoF("Saving file to {}", realSaveLoc);
		if (!FileRW::WriteFile(realSaveLoc, contents))
		{
			textEditorAskSaveFileAs(
				contents,
				std::format("Couldn't save to {}. YES to choose a different location, NO to discard file", realSaveLoc)
			);
		}
	}
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

static int s_TextEdDebuggerCurrentLine = 0;

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
			TextEdOpenFiles.push_back(UNSAVEDTAG);
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

		if (ImGui::MenuItem("Save", NULL, nullptr))
		{
			TextEditorFileModified = true; // forces a save
			textEditorSaveFile();
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

	for (size_t i = 0; i < TextEdOpenFiles.size() + 1; i++)
	{
		ImGui::PushID((int)i);

		std::string label = TextEdOpenFiles.size() > i ? TextEdOpenFiles[i] : "+";
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
			TextEditorFileModified = false;

			if (i == TextEdOpenFiles.size())
				TextEdOpenFiles.push_back(UNSAVEDTAG);

			Memory::Free(TextEditorEntryBuffer);
			TextEditorEntryBuffer = nullptr;
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
				TextEdOpenFiles.push_back(UNSAVEDTAG);
				TextEdCurrentFileTab = 0;
			}
		}

		TextEditorFileStream = new std::fstream(FileRW::MakePathCwdRelative(TextEdOpenFiles[TextEdCurrentFileTab]));
		
		std::string scriptContents = "";

		if (!(*TextEditorFileStream) || !TextEditorFileStream->is_open())
		{
			TextEditorEntryBuffer = BufferInitialize(512);
			TextEditorEntryBufferCapacity = 512;

			if (TextEdOpenFiles[TextEdCurrentFileTab] != UNSAVEDTAG)
			{
				std::string failedFileName = TextEdOpenFiles[TextEdCurrentFileTab];
				if (strlen(TextEditorEntryBuffer) > 0)
				{
					TextEdOpenFiles.push_back(UNSAVEDTAG);
					TextEdCurrentFileTab = TextEdOpenFiles.size() - 1;
				}

				copyStringToBuffer(
					TextEditorEntryBuffer,
					"Couldn't read file:\n" + failedFileName,
					512
				);
				setErrorMessage(std::format(
					"File '{}' couldn't be opened",
					failedFileName
				));
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

			TextEditorFileModified = false;
		}
	}
	
	if (TextEditorEntryBuffer)
	{
		ImVec2 startPos = ImGui::GetCursorPos();
		float textYSize = ImGui::CalcTextSize("").y;

		if (s_TextEdDebuggerCurrentLine != 0)
		{
			ImGui::SetCursorPos({ startPos.x, startPos.y + textYSize * (s_TextEdDebuggerCurrentLine - 1) });

			TextureManager* texManager = TextureManager::Get();
			ImGui::ImageWithBg(
				texManager->GetTextureResource(1).GpuId,
				ImVec2(textYSize, textYSize),
				{},
				{},
				{},
				ImVec4(1.f, 0.f, 0.f, 1.f)
			);
		}

		int line = 0;

		for (size_t i = 0; i < TextEditorEntryBufferCapacity; i++)
		{
			if (TextEditorEntryBuffer[i] == '\0')
				break;

			if (TextEditorEntryBuffer[i] == '\n')
			{
				line++;
				// set the position precisely to avoid inaccuracy from padding
				ImGui::SetCursorPos({ startPos.x, startPos.y + textYSize * (line - 1) });
				ImGui::Text("%d", line);
			}
		}

		ImGui::SetCursorPos(startPos + ImVec2(textYSize * 2, 0.f));

		bool changed = ImGui::InputTextMultiline(
			"##",
			TextEditorEntryBuffer,
			TextEditorEntryBufferCapacity,
			ImVec2(ImGui::GetContentRegionAvail().x, (line + 2) * textYSize * 1.1f)
		);
		TextEditorFileModified = TextEditorFileModified || changed;
	}

	ImGui::End();
}

static void uniformsEditor(
	std::unordered_map<std::string, Reflection::GenericValue>& Uniforms,
	int* Selection, const char* Name
)
{
	ZoneScopedC(tracy::Color::DarkSeaGreen);
	ImGui::BeginChild(Name, ImVec2(), ImGuiChildFlags_Borders);

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
		Name,
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

	ImGui::EndChild();
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

		static int MainUniformSelectionIdx = -1;
		static int InheritedUniformSelectionIdx = -1;

		ImGui::Text("Inherit variables of: ");
		ImGui::SameLine();

		curItem.UniformsAncestor.reserve(64);

		ImGui::InputText("##", &curItem.UniformsAncestor);

		std::unordered_map<std::string, Reflection::GenericValue> mainUniforms = curItem.DefaultUniforms;
		std::unordered_map<std::string, Reflection::GenericValue> inheritedUniforms;

		if (curItem.UniformsAncestor.size() > 0)
		{
			const ShaderProgram& ancestor = shdManager->GetShaderResource(shdManager->LoadFromPath(curItem.UniformsAncestor));
			// do NOT show inherited uniforms to the developer unless they are modified to
			// reduce clutter
			for (const auto& it : ancestor.DefaultUniforms)
				if (const auto& cit = mainUniforms.find(it.first);
					cit != mainUniforms.end() && cit->second == it.second
				)
				{
					inheritedUniforms[it.first] = it.second;
					mainUniforms.erase(cit);
				}
		}

		uniformsEditor(mainUniforms, &MainUniformSelectionIdx, "Uniforms");

		if (inheritedUniforms.size() > 0)
			uniformsEditor(inheritedUniforms, &InheritedUniformSelectionIdx, "Inherited Uniforms");

		for (const auto& it : mainUniforms)
			curItem.DefaultUniforms[it.first] = it.second;

		for (const auto& it : inheritedUniforms)
			curItem.DefaultUniforms[it.first] = it.second;

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

	uniformsEditor(curItem.Uniforms, &SelectedUniformIdx, "Variables");

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

static std::vector<ObjectHandle> Selections;
static std::vector<ObjectHandle> VisibleTree;
static std::vector<ObjectHandle> VisibleTreeWip;
static ObjectHandle LastSelected;
static ObjectHandle ObjectInsertionTarget = nullptr;
static bool IsPickingObject = false;
static std::vector<ObjectHandle> PickerTargets;
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

typedef void(*ContextActionMenuHandlerFunc)(void);

static ContextActionMenuHandlerFunc ContextMenuActionHandlers[] =
{
	[]()
	{
		std::vector<ObjectHandle> newSelections;
		newSelections.reserve(Selections.size());
		
		for (const ObjectHandle& sel : Selections)
			newSelections.push_back(sel->Duplicate());
	
		Selections = newSelections;
	},

	[]()
	{
		for (const ObjectHandle& sel : Selections)
			sel->Destroy();
				
		Selections.clear();
	},

	[]()
	{
		std::vector<GameObject*> sels;
		std::string sceneName = "";

		for (const ObjectHandle& sel : Selections)
		{
			sels.push_back(sel.Dereference());
			sceneName += sel->Name + "_";
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

					std::vector<ObjectRef> roots = SceneFormat::Deserialize(contents, &readSuccess);

					if (!readSuccess)
					{
						setErrorMessage(SceneFormat::GetLastErrorString());
						return;
					}

					assert(Selections[0].Dereference());

					for (ObjectRef r : roots)
						r->SetParent(Selections[0].Dereference());
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
		for (const ObjectHandle& sel : Selections)
			if (EcScript* s = sel->GetComponent<EcScript>())
				invokeTextEditor(s->SourceFile);
	},

	[]()
	{
		for (const ObjectHandle& sel : Selections)
			if (EcScript* s = sel->GetComponent<EcScript>())
				s->Reload();
	}
};

static bool isInSelections(const ObjectHandle& obj)
{
	return std::find(Selections.begin(), Selections.end(), obj) != Selections.end();
}

static std::vector<ContextMenuAction> getPossibleActionsForSelections()
{
	std::vector<ContextMenuAction> actions;
	actions.reserve(4);

	bool gotScript = false;

	for (const ObjectHandle& sel : Selections)
	{
		if (!gotScript)
			if (sel->GetComponent<EcScript>())
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
			for (const ObjectHandle& target : PickerTargets)
				target->SetPropertyValue(PickerTargetPropName, nodeClicked->ToGenericValue());
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
	{
		ObjectHandle tempNc = nodeClicked;

		if (const auto& it = std::find(Selections.begin(), Selections.end(), tempNc); it != Selections.end())
			Selections.erase(it);
		else
			Selections.push_back(nodeClicked);
	}

	else if (ImGui::GetIO().KeyShift)
	{
		if (LastSelected.Reference.Referred() && LastSelected.Dereference() != nodeClicked)
		{
			if (!isInSelections(nodeClicked))
				Selections.push_back(nodeClicked);

			auto start = std::find(VisibleTree.begin(), VisibleTree.end(), LastSelected);
			auto end = std::find(VisibleTree.begin(), VisibleTree.end(), ObjectHandle(nodeClicked));

			if (start > end)
			{
				// doesn't seem to work unless it is copied fsr
				std::vector<ObjectHandle>::iterator temp = end;
				end = start;
				start = temp;
			}

			// 25/01/2025 this feels weird, like surely there's a better way
			// to iterate between `start` and `end`
			for (const ObjectHandle& middle : std::span<ObjectHandle>(start, end))
				if (!isInSelections(middle))
					Selections.push_back(middle);
		}
		else
			Selections = { nodeClicked };
	}
	else
		Selections = { nodeClicked };

	LastSelected = nodeClicked;
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

	current->ForEachChild([&](GameObject* object)
	{
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
			| ImGuiTreeNodeFlags_AllowOverlap
			| ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_DrawLinesFull;

		// make the insert button have better contrast
		if (isInSelections(object) && object != InsertObjectButtonHoveredOver)
			flags |= ImGuiTreeNodeFlags_Selected;

		if (object->Children.empty())
			flags |= ImGuiTreeNodeFlags_Leaf;

		ImGui::AlignTextToFramePadding();

		ReflectorRef primaryComponent = object->Components[0];
		if (primaryComponent.Type == EntityComponent::Transform && object->Components.size() > 1)
			primaryComponent = object->Components[1];
		
		Texture tex = getIconForComponent(primaryComponent.Type);

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
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
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
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			openInserter = true;
		
		ImGui::SameLine();
		ImGui::TextUnformatted(object->Name.c_str());

		if (ImGui::IsItemClicked())
			nodeClicked = object;
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
			isHovered = true;
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			openInserter = true;

		VisibleTreeWip.push_back(object);

		if (openInserter || ImGui::IsMouseClicked(ImGuiMouseButton_Right))
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

		if (object == (ObjectInsertionTarget.HasValue() ? ObjectInsertionTarget.Dereference() : nullptr))
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
						Selections = { newObject };

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

		return true;
	});

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

		std::string fulltip = IsObjectProp ? "\n(CTRL+LClick to select, CTRL+RClick to set to nil)" : "";
		
		if (pcit != PropTips.end())
			fulltip = pcit->second + fulltip;

		ImGui::SetItemTooltip("%s", fulltip.data());
	}
}

void InlayEditor::UpdateAndRender(double DeltaTime)
{
	ZoneScopedC(tracy::Color::DarkSeaGreen);

	if (!ExplorerRoot.Reference.Referred())
		ExplorerRoot = GameObject::GetObjectById(GameObject::s_DataModel);

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

	ImGui::BeginChild("HierarchyChildWindow", hrchChildWinSzOverride, ImGuiChildFlags_Borders);

	VisibleTreeWip.clear();
	recursiveIterateTree(ExplorerRoot);
	
	VisibleTree = VisibleTreeWip;
	
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
		for (const ObjectHandle& sel : Selections)
			idSum += sel->ObjectId;

		ImGui::PushID(idSum);

		ImGui::BeginChild("PropertiesEditor", ImVec2(), ImGuiChildFlags_Borders);

		std::string sepStr = "Properties of " + std::to_string(Selections.size()) + " objects";
		if (Selections.size() == 1)
			sepStr = std::format(
				"Properties of {}",
				Selections[0]->Name
			);

		ImGui::SeparatorText(sepStr.c_str());

		static bool ShowComponents = false;
		if (ImGui::IsItemClicked())
			ShowComponents = !ShowComponents;

		ImGui::PushID(static_cast<uint32_t>(115111116109));

		std::set<EntityComponent> components;

		for (const ObjectHandle& sel : Selections)
			for (const ReflectorRef& ref : sel->Components)
				components.insert(ref.Type);

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

		std::unordered_map<std::string_view, std::pair<const Reflection::PropertyDescriptor*, Reflection::GenericValue>> props;
		std::unordered_map<std::string_view, bool> conflictingProps;

		for (const ObjectHandle& sel : Selections)
		{
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
			const std::pair<const Reflection::PropertyDescriptor*, Reflection::GenericValue> propItem = propIt.second;

			const std::string_view& propName = propIt.first;
			const Reflection::PropertyDescriptor* propDesc = propItem.first;
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

					if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
					{
						if (ImGui::GetIO().KeyCtrl)
						{
							if (referenced)
							{
								GameObject* target = GameObject::FromGenericValue(newVal);
								Selections = { target };
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
					else if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
						if (ImGui::GetIO().KeyCtrl)
						{
							for (const ObjectHandle& object : PickerTargets)
								object->SetPropertyValue(PickerTargetPropName, {});
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
					for (const ObjectHandle& sel : Selections)
						sel->SetPropertyValue(propName, newVal);
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

void InlayEditor::SetExplorerRoot(const ObjectHandle NewRoot)
{
	ExplorerRoot = NewRoot;
}

void InlayEditor::SetExplorerSelections(const std::vector<ObjectHandle>& NewSelections)
{
	Selections = NewSelections;
}

void InlayEditor::Shutdown()
{
	MtlPreviewCamera = nullptr;
	ExplorerRoot = nullptr;
	Selections.clear();
	VisibleTree.clear();
	VisibleTreeWip.clear();
	PickerTargets.clear();
	LastSelected.Clear();
	
	if (TextEditorEntryBuffer)
		textEditorSaveFile();
}

#include <imgui/backends/imgui_impl_opengl3.h>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <lualib.h>
#include <thread>
#include <cmath>

#include "Engine.hpp"

static void debugVariable(lua_State* L)
{
	ZoneScoped;
	luaL_checkstack(L, 2, "debugVariable");

	std::string varname;
	switch (lua_type(L, -2))
	{
	case LUA_TLIGHTUSERDATA:
	{
		varname = "(light userdata)";
		break;
	}
	case LUA_TSTRING:
	{
		varname = luaL_checkstring(L, -2);
		break;
	}
	default:
	{
		const char* ktn = luaL_typename(L, -2);
		varname = std::format("{} ({})", luaL_tolstring(L, -2, nullptr), ktn);
		lua_pop(L, 1);
	}
	}

	const char* vtn = luaL_typename(L, -1);
	constexpr ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DrawLinesFull;

	switch (lua_type(L, -1))
	{
	case LUA_TFUNCTION:
	{
		lua_Debug ar{};
		lua_getinfo(L, -1, "sluan", &ar);
		ar.name = ar.name ? ar.name : "<anonymous>";

		std::string reassigned = ar.name != varname ? std::format("{} ", ar.name) : "";

		if (ImGui::TreeNodeEx(varname.c_str(), NodeFlags, "%s: %s(function)", varname.c_str(), reassigned.c_str()))
		{
			ImGui::Text("%s function '%s'", ar.what, ar.name ? ar.name : "<anonymous>");

			if (ar.linedefined != -1 || strcmp(ar.short_src, "[C]") != 0 || !ar.isvararg || ar.nupvals != 0)
			{
				ImGui::Text("Defined at line %i", ar.linedefined);
				ImGui::Text("Of %s", ar.short_src);
				ImGui::Text("Is variadic: %s", ar.isvararg ? "true" : "false");

				if (!ar.isvararg)
					ImGui::Text("# Parameters: %i", ar.nparams);
				
				ImGui::Text("# Upvalues: %i", ar.nupvals);
			}

			ImGui::TreePop();
		}

		break;
	}

	case LUA_TTABLE:
	{
		if (ImGui::TreeNodeEx(varname.c_str(), NodeFlags, "%s: (table)", varname.c_str()))
		{
			if (lua_getmetatable(L, -1))
			{
				lua_pushstring(L, "(metatable)");
				lua_pushvalue(L, -2);
				debugVariable(L);
				
				lua_pop(L, 3);
			}

			lua_pushnil(L);
			while (lua_next(L, -2))
			{
				debugVariable(L);

				lua_pop(L, 1);
			}

			ImGui::TreePop();
		}

		break;
	}

	case LUA_TLIGHTUSERDATA:
	{
		ImGui::Text("%s: (light userdata)", varname.c_str());
		break;
	}

	default:
	{
		if (ImGui::TreeNodeEx(
			varname.c_str(),
			NodeFlags | ImGuiTreeNodeFlags_Leaf,
			"%s: %s (%s)",
			varname.c_str(), luaL_tolstring(L, -1, nullptr), vtn
		))
			ImGui::TreePop();
			
		lua_pop(L, 1);
	}
	}
}

static void debugBreakHook(lua_State* L, lua_Debug* ar, bool HasError, bool)
{
	ZoneScoped;

	luaL_checkstack(L, 20, "debugger");
	int initialStatus = L->status;

	std::string errorMessage = HasError ? luaL_checkstring(L, -1) : "<No error message>";
	std::string vmName;
	lua_getglobal(L, "_VMNAME");
	vmName = luaL_tolstring(L, -1, nullptr);
	lua_pop(L, 1);

	Engine* engine = Engine::Get();

	ImGuiContext* debuggerContext = ImGui::CreateContext();
	ImGuiContext* prevContext = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(debuggerContext);

	PHX_ENSURE_MSG(ImGui_ImplSDL3_InitForOpenGL(
		engine->Window,
		engine->RendererContext.GLContext
	), "`ImGui_ImplSDL3_InitForOpenGL` failed");
	PHX_ENSURE_MSG(ImGui_ImplOpenGL3_Init("#version 460"), "`ImGui_ImplOpenGL3_Init` failed");

	double debuggerLastSecond = GetRunningTime();
	double debuggerLastFrame = GetRunningTime();
	int framesPerSecond = 0;
	int framesInSecond = 0;
	bool develUI = false;
	bool quietBg = true;
	bool running = true;

	bool wasTextEdEnabled = TextEditorEnabled;

	int li = 0;
	lua_getinfo(L, li, "slnfu", ar);
	while (strcmp(ar->short_src, "[C]") == 0)
	{
		lua_pop(L, 1);
		li++;

		if (!lua_getinfo(L, li, "slnfu", ar))
		{
			lua_getinfo(L, 0, "slnfu", ar);
			break;
		}
	}

	int currfuncindex = lua_gettop(L);
	invokeTextEditor(ar->short_src);

	while (running)
	{
		ZoneScopedN("DebuggerFrame");
		double dt = GetRunningTime() - debuggerLastFrame;
		debuggerLastFrame = GetRunningTime();

		SDL_Event event;
		while (SDL_PollEvent(&event) != 0)
		{
			engine->RendererContext.AccumulatedDrawCallCount = 0;
			ImGui_ImplSDL3_ProcessEvent(&event);

			switch (event.type)
			{
			
			case SDL_EVENT_QUIT:
			{
				running = false;
				L->singlestep = false;
				L->global->cb.debugstep = nullptr;

				engine->Close();
				
				break;
			}
		
			case SDL_EVENT_WINDOW_RESIZED:
			{
				int NewSizeX = event.window.data1;
				int NewSizeY = event.window.data2;
			
				// Only call ChangeResolution if the new resolution is actually different
				//if (NewSizeX != this->WindowSizeX || NewSizeY != this->WindowSizeY)
					engine->OnWindowResized(NewSizeX, NewSizeY);
			
				break;
			}
			default: {}
			}
		}

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		glClear(GL_COLOR_BUFFER_BIT);

		if (ImGui::Begin("Debugger"))
		{
			if (HasError)
				ImGui::Text("Exception thrown");
			else
				ImGui::Text("Breakpoint hit");
				
			ImGui::Text("Script: %s", ar->short_src);
			ImGui::Text("Line: %i", ar->currentline);
			ImGui::Text("In: %s", ar->name ? ar->name : "<anonymous function>");
			ImGui::TextUnformatted(errorMessage.c_str());
			ImGui::Text("VM: %s", vmName.c_str());

			if (ImGui::Button("Resume (F5)") || ImGui::IsKeyDown(ImGuiKey_F5))
				running = false;

			static bool s_WasF8Down = false;
			bool isF8Down = ImGui::IsKeyDown(ImGuiKey_F8) && running;

			if (isF8Down)
			{
				if (s_WasF8Down)
					isF8Down = false;

				s_WasF8Down = true;
			}
			else
				s_WasF8Down = false;

			if (ScriptEngine::L::DebugBreak && (ImGui::Button("Single-step (F8)") || isF8Down))
			{
				static int s_TargetLine = 0;
				static int s_PrevLine = 0;
				s_TargetLine = ar->currentline + 1;
				s_PrevLine = ar->currentline;

				static int s_BreakOnNext = false;
				s_BreakOnNext = false;

				L->global->cb.debugstep = [](lua_State* L, lua_Debug* ar)
					{
						if (s_BreakOnNext)
							lua_break(L);

						if (ar->currentline >= s_TargetLine)
							s_BreakOnNext = true;

						if (ar->currentline < s_PrevLine)
							lua_break(L);
					};
				
				lua_singlestep(L, true);
				L->status = LUA_BREAK;

				if (lua_resume(L, L, 0) == LUA_OK)
				{
					// got to the end of the script
					running = false;
				}
				
				L->global->cb.debugstep = nullptr;
				lua_singlestep(L, false);

				int li = 0;
				lua_getinfo(L, li, "slnu", ar);
				while (strcmp(ar->short_src, "[C]") == 0)
				{
					li++;
				
					if (!lua_getinfo(L, li, "slnu", ar))
					{
						lua_getinfo(L, 0, "slnu", ar);
						break;
					}
				}
			}

			ImGui::Checkbox("All Developer UIs", &develUI);
			ImGui::Checkbox("Quiet Background", &quietBg);
			ImGui::Text("Debugger %i FPS / %.2fms", framesPerSecond, dt);

			if (ScriptEngine::L::DebugBreak)
			{
				if (ImGui::Button("Detach Debugger for this VM"))
				{
					ScriptEngine::L::DebugBreak = nullptr;
					L->global->cb.debugbreak = nullptr;
					L->global->cb.debugprotectederror = nullptr;
					L->global->cb.debugstep = nullptr;
				}
			}
			else
			{
				ImGui::Text("The Debugger has been detached for VM %s.", vmName.c_str());
			}
		}
		ImGui::End();

		s_TextEdDebuggerCurrentLine = ar->currentline;

		if (develUI)
			engine->OnFrameRenderGui.Fire(dt);
		else
			renderTextEditor();

		if (ImGui::Begin("Watch"))
		{
			static int Section = 0;
			ImGui::Combo("Variables", &Section, "Locals\0Upvalues\0Globals\0Environment\0Registry\0");

			ImGui::BeginChild("VariablesSection", ImVec2(), ImGuiChildFlags_Borders);
			L->status = LUA_OK; // avoid hitting assertion due to potential calls to `__tostring` metamethods
			
			switch (Section)
			{
			case 0:
			{
				for (int l = 0; l < L->ci - L->base_ci; l++)
				{
					ImGui::Text("---LEVEL %i---", l);
					ImGui::PushID(l);

					for (int i = 1; i < 256; i++)
					{
						luaL_checkstack(L, 3, "get local");
						const char* name = lua_getlocal(L, l, i);

						if (!name)
							break; // TODO are they contiguous?

						lua_pushstring(L, name);
						lua_pushvalue(L, -2);
						debugVariable(L);
						lua_pop(L, 2);

						lua_pop(L, 1);
					};

					ImGui::PopID();
				}

				break;
			}
			case 1:
			{
				for (int i = 1; i < ar->nupvals; i++)
				{
					const char* name = lua_getupvalue(L, currfuncindex, i);
				
					if (!name)
						break;
				
					lua_pushstring(L, name);
					lua_pushvalue(L, -2);
					debugVariable(L);
					lua_pop(L, 2);
				
					lua_pop(L, 1);
				}

				break;
			}
			case 2:
			{
				if (lua_getmetatable(L, -1))
				{
					lua_pushstring(L, "(metatable)");
					lua_pushvalue(L, -2);
					debugVariable(L);

					lua_pop(L, 3);
				}

				lua_pushnil(L);
				while (lua_next(L, LUA_GLOBALSINDEX))
				{
					debugVariable(L);

					lua_pop(L, 1);
				}
				
				break;
			}
			case 3:
			{
				if (lua_getmetatable(L, -1))
				{
					lua_pushstring(L, "(metatable)");
					lua_pushvalue(L, -2);
					debugVariable(L);
					
					lua_pop(L, 3);
				}

				lua_pushnil(L);
				while (lua_next(L, LUA_ENVIRONINDEX))
				{
					debugVariable(L);

					lua_pop(L, 1);
				}

				break;
			}
			case 4:
			{
				if (lua_getmetatable(L, -1))
				{
					lua_pushstring(L, "(metatable)");
					lua_pushvalue(L, -2);
					debugVariable(L);
					
					lua_pop(L, 3);
				}

				lua_pushnil(L);
				while (lua_next(L, LUA_REGISTRYINDEX))
				{
					debugVariable(L);

					lua_pop(L, 1);
				}

				break;
			}
			[[unlikely]] default: { assert(false); }

			}
			L->status = initialStatus;
			ImGui::EndChild();
		}
		ImGui::End();

		if (ImGui::Begin("Callstack"))
		{
			lua_Debug car;
			for (int i = 0; lua_getinfo(L, i, "sln", &car); i++)
			{
				if (car.currentline > 0)
				{
					if (ImGui::Button(std::format(
						"{}:{} in {}",
						car.short_src, car.currentline, car.name ? car.name : "<anonmyous>"
					).c_str()))
					{
						invokeTextEditor(car.short_src);
						lua_getinfo(L, i, "sln", ar);
					}
				}
				else
					ImGui::Text("%s in %s", car.short_src, car.name);
			}
		}
		ImGui::End();

		if (!quietBg)
		{
			EcCamera* sceneCam = engine->WorkspaceRef->GetComponent<EcWorkspace>()->GetSceneCamera()->GetComponent<EcCamera>();

			engine->RendererContext.DrawScene(
				engine->CurrentScene,
				sceneCam->GetMatrixForAspectRatio((float)engine->WindowSizeX / engine->WindowSizeY),
				sceneCam->Transform,
				GetRunningTime(),
				engine->DebugWireframeRendering
			);
		}

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		engine->RendererContext.SwapBuffers();

		framesInSecond++;
		if (GetRunningTime() - debuggerLastSecond >= 1.f)
		{
			debuggerLastSecond = GetRunningTime();
			framesPerSecond = framesInSecond;
			framesInSecond = 0;
		}

		std::this_thread::sleep_for(std::chrono::duration<double>(std::clamp(0.01 - dt, 0.0, 0.01)));
		Memory::FrameFinish();
	}

	L->status = initialStatus;

	TextEditorEnabled = wasTextEdEnabled;
	s_TextEdDebuggerCurrentLine = 0;

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::SetCurrentContext(prevContext);
	ImGui::DestroyContext(debuggerContext);
}
