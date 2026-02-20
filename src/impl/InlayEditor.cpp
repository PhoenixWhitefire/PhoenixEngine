#define GLM_ENABLE_EXPERIMENTAL
#define IMGUI_DEFINE_MATH_OPERATORS

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_internal.h>
#include <tracy/Tracy.hpp>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <ImGuiColorTextEdit/TextEditor.h>
#include <luau/VM/src/lstate.h>
#include <tinyfiledialogs.h>
#include <lualib.h>
#include <fstream>
#include <set>

#include "InlayEditor.hpp"

#include "asset/MaterialManager.hpp"
#include "asset/TextureManager.hpp"
#include "asset/SceneFormat.hpp"
#include "asset/MeshProvider.hpp"

#include "component/Camera.hpp"

#include "GlobalJsonConfig.hpp"
#include "Utilities.hpp"
#include "UserInput.hpp"
#include "History.hpp"
#include "Memory.hpp"
#include "FileRW.hpp"
#include "Log.hpp"

bool InlayEditor::DidInitialize = false;

constexpr uint32_t OBJECT_NEW_CLASSNAME_BUFSIZE = 16;
constexpr uint32_t MATERIAL_NEW_NAME_BUFSIZE = 64;
constexpr uint32_t MATERIAL_TEXTUREPATH_BUFSIZE = 128;
constexpr char MATERIAL_NEW_NAME_DEFAULT[] = "newmaterial";

struct TextEditorTab
{
	TextEditor Editor;
	std::string FilePath;
	std::ifstream* FileStream = nullptr;
	std::vector<int> Breakpoints;
	int JumpToLine = 0; // line to jump to, `0` to not jump
	int DebuggerCurrentLine = 0;
	bool WasPreviouslyVisible = false;
	bool SetUIFocus = false;
};
static std::vector<TextEditorTab> s_TextEditors;
static auto s_EditorLuauLang = TextEditor::LanguageDefinition::Lua();

static const std::string_view AddableComponents[] = {
	"Animation",
	"Camera",
	"DirectionalLight",
	"PointLight",
	"SpotLight",
	"Mesh",
	"Model",
	"ParticleEmitter",
	"RigidBody",
	"Sound",
	"Transform"
};

static nlohmann::json DefaultNewMaterial =  {
	{ "ColorMap", "textures/materials/plastic.png" },
	{ "specExponent", 32.f },
	{ "specMultiply", 0.5f }
};

static nlohmann::json DocumentationJson = nlohmann::json::object();
static nlohmann::json ObjectDocCommentsJson = nlohmann::json::object();
static nlohmann::json ApiDumpJson = nlohmann::json::object();
static std::string DatatypesDocPrologue;
static std::string LibrariesDocPrologue;
static std::string GlobalsDocPrologue;

static std::unordered_map<std::string, std::string> ClassIcons;

static bool ExplorerShouldSeekToCurrentSelection = false;

static GpuFrameBuffer MtlEditorPreview;
static Scene MtlPreviewScene = Scene{
	// cube
	.RenderList = {
		RenderItem{
			.RenderMeshId = 0u,
			.Transform = glm::mat4(1.f),
			.Size = glm::vec3(1.f),
			.MaterialId = 0u,
			.TintColor = glm::vec3(1.f),
			.Transparency = 0.f,
			.MetallnessFactor = 0.f,
			.RoughnessFactor = 0.f,
			.FaceCulling = FaceCullingMode::BackFace
		}
	},
	// light source
	.LightingList = {
		LightItem{
			.Position = glm::vec3{ 0.57f, 0.57f, 0.57f },
			.LightColor = glm::vec3{ 1.f, 1.f, 1.f },
			.Type = LightType::Directional,
			.Shadows = false
		}
	},
	.UsedShaders = {} // used shaders
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
static void debugBreakHook(lua_State*, lua_Debug*, ScriptEngine::L::DebugBreakReason);
static void debuggerLeaveCallback();

static bool canBreakLineAtChar(char c)
{
	return c == ' ' || c == ',' || c == '.' || c == '!' || c == '?';
}

static std::string addLinebreaks(std::string String, const size_t MaxCharactersBeforeLinebreak)
{
	if (String.size() < MaxCharactersBeforeLinebreak)
		return String;

	if (MaxCharactersBeforeLinebreak < 10)
		return String;

	size_t sinceLinebreak = 0;

	for (size_t i = 0; i < String.size(); i++)
	{
		if (sinceLinebreak == MaxCharactersBeforeLinebreak)
		{
			if (canBreakLineAtChar(String[i]))
				String.insert(String.begin() + i, '\n');
			else
			{
				bool didBreak = false;

				for (size_t j = i; j > 0; j--)
					if (canBreakLineAtChar(String[j]))
					{
						String.insert(String.begin() + j, '\n');
						didBreak = true;
						break;
					}

				if (!didBreak)
					String.insert(i, "-\n");
			}

			sinceLinebreak = 0;
		}

		sinceLinebreak++;
	}

	return String;
}

static std::string getDescriptionAsString(const nlohmann::json& DescriptionJson, size_t nCharsPerLine)
{
	if (DescriptionJson.is_string())
	{
		return addLinebreaks(DescriptionJson, nCharsPerLine);
	}
	else if (DescriptionJson.is_object())
	{
		return getDescriptionAsString(DescriptionJson["Description"], nCharsPerLine);
	}
	else if (DescriptionJson.is_array())
	{
		std::string desc;

		for (auto descit = DescriptionJson.begin(); descit != DescriptionJson.end(); descit++)
			desc += "* " + addLinebreaks(descit.value(), nCharsPerLine) + "\n";

		desc = desc.substr(0, desc.size() - 1);

		return desc;
	}
	else if (DescriptionJson.is_null())
		return "Standard Luau";

	else
		return std::format("Unexpected description type '{}'", DescriptionJson.type_name());
}

static std::string findLuauTypeFromDocumentation(const nlohmann::json& Docs, const std::string& RuntimeType)
{
	if (!Docs.is_object())
		return RuntimeType;

	if (const auto& it = Docs.find("In"); it != Docs.end())
	{
		std::string inStr;
		if (it.value().is_string())
		{
			inStr = (std::string)it.value();
		}
		else
		{
			for (size_t i = 0; i < it.value().size(); i++)
			{
				const nlohmann::json& v = it.value()[i];
				std::string vstr = v.is_string() ? (std::string)v : (std::string)v[0];
				inStr = vstr + ", ";
			}

			inStr = inStr.substr(0, inStr.size() - 2);
		}

		std::string ty = "( " + inStr + " )";

		if (const auto& oit = Docs.find("Out"); oit != Docs.end())
			ty += " -> ( " + (std::string)oit.value() + " )";

		return ty;
	}

	if (const auto& it = Docs.find("Out"); it != Docs.end())
		return "() -> ( " + (std::string)it.value() + " )";

	if (const auto& it = Docs.find("Type"); it != Docs.end())
		return (std::string)it.value();

	return RuntimeType;
}

void InlayEditor::Initialize(Renderer* renderer)
{
	MtlEditorPreview.Initialize(256, 256);
	MtlPreviewRenderer = renderer;
	MtlPreviewCamera = GameObject::Create("Camera");
	MtlPreviewCamera->Name = "MtlPreviewCamera";

	EcCamera* cc = MtlPreviewCamera->FindComponent<EcCamera>();
	cc->SetWorldTransform(MtlPreviewCamDefaultRotation * MtlPreviewCamOffset);
	cc->FieldOfView = 50.f;

	try
	{
		bool readSuccess = true;
		std::string contents = FileRW::ReadFile("@cwd/wikigen/doc-comments.json", &readSuccess);

		if (readSuccess)
			DocumentationJson = nlohmann::json::parse(contents);
		else
			Log::Warning("Documentation tooltips will not be available (`doc-comments.json` could not be read)");
	}
	catch (const nlohmann::json::parse_error& Err)
	{
		Log::ErrorF("Parse error reading doc comments for Editor: {}", Err.what());
	}

	if (!DocumentationJson.is_null())
	{
		ObjectDocCommentsJson = DocumentationJson["GameObject"];
		
		if (ObjectDocCommentsJson.is_null())
		{
			Log::Error("`doc-comments.json` malformed");

			ObjectDocCommentsJson["Base"] = {};
			ObjectDocCommentsJson["Components"] = {};
		}
	}

	try
	{
		bool readSuccess = false;
		std::string contents = FileRW::ReadFile("@cwd/apidump.json", &readSuccess);

		if (readSuccess)
			ApiDumpJson = nlohmann::json::parse(contents);
		else
			Log::Warning("Failed to read API Dump, some documentation information may be unavailable");
	}
	catch(const nlohmann::json::parse_error& e)
	{
		Log::ErrorF("Parse error reading API Dump: {}", e.what());
	}

	bool prologueFound = true;
	DatatypesDocPrologue = FileRW::ReadFile("@cwd/wikigen/datatypes-prologue.md", &prologueFound);
	PHX_CHECK(prologueFound && "Datatypes prologue - @cwd/wikigen/datatypes-prologue.md");

	LibrariesDocPrologue = FileRW::ReadFile("@cwd/wikigen/libraries-prologue.md", &prologueFound);
	PHX_CHECK(prologueFound && "Libraries prologue - @cwd/wikigen/libraries-prologue.md");

	GlobalsDocPrologue = FileRW::ReadFile("@cwd/wikigen/globals-prologue.md", &prologueFound);
	PHX_CHECK(prologueFound && "Globals prologue - @cwd/wikigen/globals-prologue.md");

	ScriptEngine::L::DebugBreak = &debugBreakHook;
	ScriptEngine::L::LeaveDebugger = debuggerLeaveCallback;

	ObjectHandle tempdm = GameObject::Create("DataModel");
	ObjectRef tempwp = GameObject::Create("Workspace");
	tempwp->SetParent(tempdm);
	GameObject::s_DataModel = tempdm->ObjectId;

	s_EditorLuauLang.mTokenRegexStrings[5].first = "[a-zA-Z_][a-zA-Z0-9_\\.]*"; // allow identifiers to have `.` so that `task.defer` etc can match as one single token
	s_EditorLuauLang.mTokenRegexStrings.push_back({ "\\`(?:\\.|[^\\`{]|\\{[^}]*\\})*\\`", TextEditor::PaletteIndex::String }); // string interpolation

	s_EditorLuauLang.mKeywords.emplace("continue");
	s_EditorLuauLang.mIdentifiers.clear();
	s_EditorLuauLang.mName = "Luau";

	const nlohmann::json& scriptEnvDocs = DocumentationJson.value("ScriptEnv", nlohmann::json::object());

	const nlohmann::json& datatypesDocs = scriptEnvDocs.value("Datatypes", nlohmann::json::object());
	const nlohmann::json& librariesDocs = scriptEnvDocs.value("Libraries", nlohmann::json::object());
	const nlohmann::json& globalsDocs = scriptEnvDocs.value("Globals", nlohmann::json::object());

	lua_State* L = ScriptEngine::L::Create("EnvironmentScraper");
	lua_pushnil(L);

	while (lua_next(L, LUA_ENVIRONINDEX))
	{
		std::string name = luaL_tolstring(L, -2, nullptr);
		lua_pop(L, 1);

		nlohmann::json mainDescriptionData = globalsDocs.find(name) != globalsDocs.end() ? globalsDocs[name]
												: librariesDocs.find(name) != librariesDocs.end() ? librariesDocs[name]
												: datatypesDocs.find(name) != datatypesDocs.end() ? datatypesDocs[name]
												: "Standard Luau";

		s_EditorLuauLang.mIdentifiers[name] = TextEditor::Identifier{
			.mDeclaration = std::format("{}: {}\n{}", name, findLuauTypeFromDocumentation(mainDescriptionData, luaL_typename(L, -1)), getDescriptionAsString(mainDescriptionData, 64))
		};

		if (lua_type(L, -1) == LUA_TTABLE && name != "_G")
		{
			auto it = librariesDocs.find(name);
			bool hasAnyDocs = true;
			bool isLibrary = true;

			if (it == librariesDocs.end())
			{
				it = datatypesDocs.find(name);
				isLibrary = false;

				if (it == datatypesDocs.end())
					hasAnyDocs = false;
			}

			lua_pushnil(L);
			while (lua_next(L, -2))
			{
				std::string innerName = luaL_tolstring(L, -2, nullptr);
				lua_pop(L, 1);

				const nlohmann::json& docs = hasAnyDocs ? (
					isLibrary ? it.value()["Members"] : it.value()["Library"]
				) : nlohmann::json::object();

				nlohmann::json descriptionData = docs.value(innerName, nlohmann::json{{ "Description", "Standard Luau" }});

				std::string fullName = std::format("{}.{}", name, innerName);

				s_EditorLuauLang.mIdentifiers[fullName] = TextEditor::Identifier{
					.mDeclaration = std::format("{}: {}\n{}", fullName, findLuauTypeFromDocumentation(descriptionData, luaL_typename(L, -1)), getDescriptionAsString(descriptionData, 64))
				};

				lua_pop(L, 1);
			}
		}

		lua_pop(L, 1);
	}

	ScriptEngine::L::Close(L);

	tempwp->Destroy();
	tempdm->Destroy();

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
	std::string cwd = std::filesystem::current_path().string() + "/";

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
	bool AskSave,
	const std::string& Contents,
	const std::string& Reason = "Do you want to save the following file?",
	bool isError = false
)
{
	if (AskSave)
	{
		std::string message = std::format(
			"{}\n\nLength: {} characters\nBody:\n{}{}",
			Reason,
			Contents.size(), Contents.substr(0, std::min((size_t)50ull, Contents.size())),
			Contents.size() > (size_t)50ull ? "\n... (truncated)" : ""
		);

		int choice = tinyfd_messageBox(
			"Save File?",
			message.c_str(),
			"yesno",
			isError ? "error" : "question",
			1
		);

		if (choice == 0)
			return false;
	}

	const char* saveTargetRaw = tinyfd_saveFileDialog(
		"Save Text Document",
		FileRW::MakePathAbsolute("@projres/").c_str(),
		0,
		nullptr,
		nullptr
	);

	if (!saveTargetRaw)
	{
		Log::Info("No file path selected in `textEditorAskSaveFileAs`");
		return false;
	}
	std::string savePath = FileRW::MakePathCwdRelative(saveTargetRaw);

	std::string errMessage;
	if (!FileRW::WriteFile(savePath, Contents, &errMessage))
	{
		Log::ErrorF("`textEditorAskSaveFileAs` save failed with {} to {}, re-prompting", savePath, errMessage);
		return textEditorAskSaveFileAs(
			true,
			Contents,
			std::format("Couldn't save to file '{}' because of error {}\nRetry?", savePath, errMessage),
			true
		);
	}

	return true;
}

static void textEditorSaveFile(TextEditorTab& Tab, bool AskSave = true)
{
	std::string contents = Tab.Editor.GetText();
	std::string textEditorFile = Tab.FilePath;

	if (textEditorFile == SAVINGTAG)
	{
		setErrorMessage("Can't... Shouldn't try and save that... How'd you do this?");
		return;
	}

	if (Tab.FileStream)
	{
		if (Tab.FileStream->is_open())
			Tab.FileStream->close();
		delete Tab.FileStream;
		Tab.FileStream = nullptr;
	}

	if (textEditorFile == UNSAVEDTAG || textEditorFile.empty())
	{
		static uint32_t ErrCount = 0;
		ErrCount++;

		Tab.FilePath = SAVINGTAG;
		if (!textEditorAskSaveFileAs(AskSave, contents))
			Tab.FilePath = UNSAVEDTAG;
	}
	else
	{
		std::string realSaveLoc = FileRW::MakePathCwdRelative(textEditorFile);

		std::string error;
		if (!FileRW::WriteFile(realSaveLoc, contents, &error))
		{
			textEditorAskSaveFileAs(
				true,
				contents,
				std::format("Couldn't save to {}: {}.\nClick YES to choose a different location, or NO to discard the file", error, realSaveLoc)
			);
		}
	}
}

static std::string textFileContentsFromPath(const std::string& Path, std::ifstream*& Stream)
{
	if (Path.find("!InlineDocument:") != std::string::npos)
		return { Path.begin() + strlen("!InlineDocument:"), Path.end() };

	if (std::filesystem::is_directory(FileRW::MakePathCwdRelative(Path)))
	{
		std::string error = std::format(
			"File '{}' couldn't be read, it is a directory",
			Path
		);

		setErrorMessage(error);
		return error;
	}

	Stream = new std::ifstream(FileRW::MakePathCwdRelative(Path));

	std::string scriptContents = "";

	if (!(*Stream) || !Stream->is_open() || Stream->bad() || Stream->fail() || Stream->tellg() > 100e6)
	{
		std::string error = std::format(
			"File '{}' couldn't be read",
			Path
		);

		setErrorMessage(error);
		return error;
	}
	else
	{
		Stream->seekg(0, std::ios::end);
		scriptContents.resize(Stream->tellg());
		Stream->seekg(0, std::ios::beg);
		Stream->read(&scriptContents[0], scriptContents.size());

		return scriptContents;
	}
}

static TextEditorTab& invokeTextEditor(const std::string& File)
{
	for (TextEditorTab& tab : s_TextEditors)
	{
		if (tab.FilePath == File)
		{
			tab.SetUIFocus = true;
			return tab;
		}
	}

	s_TextEditors.emplace_back();

	TextEditorTab& tab = s_TextEditors.back();

	if (File.find(".luau") != std::string::npos)
		tab.Editor.SetLanguageDefinition(s_EditorLuauLang);

	else if (File.find(".vert") != std::string::npos || File.find(".frag") != std::string::npos || File.find(".geom") != std::string::npos || File.find(".glsl") != std::string::npos)
		tab.Editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());

	else if (File.find(".cpp") != std::string::npos || File.find(".hpp") != std::string::npos)
		tab.Editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());

	tab.FilePath = File;
	tab.Editor.SetText(textFileContentsFromPath(File, tab.FileStream));

	return tab;
}

static void renderTextEditors()
{
	ZoneScopedC(tracy::Color::DarkSeaGreen);

	for (int index = 0; index < (int)s_TextEditors.size(); index++)
	{
		TextEditorTab& tab = s_TextEditors[index];

		if (tab.JumpToLine > 0 || tab.SetUIFocus)
		{
			ImGui::SetNextWindowFocus();
			tab.SetUIFocus = false;
		}

		bool open = true;
		bool render = ImGui::Begin(std::format("{}###TextEditor_{}", std::filesystem::path(tab.FilePath).filename().string(), index).c_str(), &open);

		if (!open)
		{
			textEditorSaveFile(tab);
			ImGui::End();

			s_TextEditors.erase(s_TextEditors.begin() + index);
			index--;

			continue;
		}

		if (!render)
		{
			ImGui::End();

			if (tab.WasPreviouslyVisible)
			{
				textEditorSaveFile(tab);
				tab.WasPreviouslyVisible = false;
			}

			continue;
		}

		tab.WasPreviouslyVisible = true;

		// "Breakpoints" just highlight the entire line in blue with the theme I've set for ImGuiColorTextEdit,
		// use that for the current line and maybe re-use the previous current-line indicator (which put a red marker on the line number)
		// as the breakpoint indicator
		if (tab.DebuggerCurrentLine > 0)
			tab.Editor.SetBreakpoints({ tab.DebuggerCurrentLine });

		tab.Editor.Render("TextEditor");

		if (tab.JumpToLine > 0)
		{
			tab.Editor.SetCursorPosition({ 0, 0 }); // force scroll
			tab.Editor.SetCursorPosition({ tab.JumpToLine - 1, 0 });
			tab.JumpToLine = 0;
		}

		ImGui::End();
	}
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
		const char* filter[] = { "*.vert", "*.frag", "*.geom" };

		const char* file = tinyfd_openFileDialog(
			"Select Shader File",
			shddir.c_str(),
			3,
			filter,
			"Shader Files",
			false
		);

		if (file)
		{
			std::string fullpath = file;
	
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
				*Target = shortpath;
			}
		}
	}
}

static void renderShaderPipelinesEditor()
{
	ZoneScopedC(tracy::Color::DarkSeaGreen);

	if (EngineJsonConfig["Tool_Shaders"] == false)
		return;

	bool open = true;

	if (!ImGui::Begin("Shader Pipelines", &open))
	{
		ImGui::End();

		return;
	}

	if (!open)
		EngineJsonConfig["Tool_Shaders"] = false;

	ShaderManager* shdManager = ShaderManager::Get();
	std::vector<ShaderProgram>& shaders = shdManager->GetLoadedShaders();

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
			const char* filter[] = { "*.png", "*.jpg", "*.jpeg" };

			const char* path = tinyfd_openFileDialog(
				"Select Texture",
				texdir.c_str(),
				3,
				filter,
				"Images",
				false
			);

			if (path)
			{
				std::string fullpath = path;
	
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
					copyStringToBuffer(CurrentPath, shortpath, MATERIAL_TEXTUREPATH_BUFSIZE);
				
					uint32_t newtexid = TextureManager::Get()->LoadTextureFromPath(shortpath);
					// i'm so silly 04/12/2024
					*TextureIdPtr = newtexid;
				}
			}
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

	if (EngineJsonConfig["Tool_Materials"] == false)
		return;

	bool open = true;

	if (!ImGui::Begin("Materials", &open))
	{
		ImGui::End();
		return;
	}

	if (!open)
		EngineJsonConfig["Tool_Materials"] = false;

	MaterialManager* mtlManager = MaterialManager::Get();
	TextureManager* texManager = TextureManager::Get();

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

	EcCamera* cc = MtlPreviewCamera->FindComponent<EcCamera>();

	if (ImGui::IsItemHovered())
	{
		if (PreviewRotStart == 0.f)
			PreviewRotStart = GetRunningTime();

		glm::mat4 transform = MtlPreviewCamDefaultRotation * glm::rotate(
			glm::mat4(1.f),
			glm::radians(static_cast<float>((GetRunningTime() - PreviewRotStart) * 45.f)),
			glm::vec3(0.f, 1.f, 0.f)
		);
		cc->SetWorldTransform(transform * MtlPreviewCamOffset);
	}
	else
	{
		cc->SetWorldTransform(MtlPreviewCamDefaultRotation * MtlPreviewCamOffset);
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
		cc->GetRenderMatrix(1.f),
		cc->GetWorldTransform(),
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
static std::vector<ObjectRef> VisibleTree;
static std::vector<ObjectRef> VisibleTreeWip;
static ObjectHandle LastSelected;
static ObjectHandle ObjectInsertionTarget = nullptr;
static bool IsPickingObject = false;
static std::vector<ObjectHandle> PickerTargets;
static std::string PickerTargetPropName;
static bool FocusRenameSelection = false;

enum class ContextMenuAction : uint8_t
{
	Duplicate = 0,
	Delete,
	SaveToFile,
	LoadFromFile,
	Rename,

	__sectionSeparator
};
static const bool ContextMenuActionRequiresSelection[] = {
	true,
	true,
	true,
	false,
	true
};

static std::vector<ContextMenuAction> ContextActionsForSelection;

static const char* ContextMenuActionStrings[] = {
	"Duplicate",
	"Delete",
	"Save to File",
	"Insert from File",
	"Rename"
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
		ExplorerShouldSeekToCurrentSelection = true;
	},

	[]()
	{
		for (const ObjectHandle& sel : Selections)
			sel->SetParent(nullptr); // We can't actually `::Destroy` it because then we won't be able to Undo it back

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
		std::string ser = SceneFormat::Serialize(sels, "SaveToFileAction_" + sceneName);
		const char* filter[] = { ".hxscene" };

		const char* path = tinyfd_saveFileDialog(
			"Save Objects",
			getFileDirectory("resources/scenes/dummy.file").c_str(),
			1,
			filter,
			"Scenes"
		);

		if (path)
		{
			std::string error;
			if (!FileRW::WriteFile(path, ser))
				setErrorMessage(std::format("Failed to save to '{}', error: {}", path, error));
		}
		else
			Log::Info("No save path select to save objects");
	},

	[]()
	{
		const char* filter[] = { "*.hxscene" };

		const char* path = tinyfd_openFileDialog(
			"Insert Objects",
			getFileDirectory("resources/scenes/dummy.file").c_str(),
			1,
			filter,
			"Scenes",
			false
		);

		if (path)
		{
			std::string fullpath = path;

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

				if (Selections.size() > 0)
				{
					for (ObjectRef r : roots)
						r->SetParent(Selections[0].Dereference());
				}
				else
				{
					for (ObjectRef r : roots)
						r->SetParent(ExplorerRoot);
				}
			}
		}
		else
			Log::Info("No file selected to insert object from");
	},

	[]()
	{
		FocusRenameSelection = true;
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

	if (Selections.size() > 0)
	{
		actions.push_back(ContextMenuAction::Rename);
		actions.push_back(ContextMenuAction::Duplicate);
		actions.push_back(ContextMenuAction::Delete);
		actions.push_back(ContextMenuAction::SaveToFile);
		actions.push_back(ContextMenuAction::LoadFromFile);
	}

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
		ExplorerShouldSeekToCurrentSelection = true;

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
				std::vector<ObjectRef>::iterator temp = end;
				end = start;
				start = temp;
			}

			// 25/01/2025 this feels weird, like surely there's a better way
			// to iterate between `start` and `end`
			for (const ObjectRef& middle : std::span<ObjectRef>(start, end))
				if (!isInSelections(middle))
					Selections.push_back(middle);
		}
		else
		{
			Selections = { nodeClicked };
		}
	}
	else
	{
		Selections = { nodeClicked };
	}

	LastSelected = nodeClicked;
}

static Texture getIconForComponent(EntityComponent Ec)
{
	TextureManager* texManager = TextureManager::Get();

	std::string componentName = std::string(s_EntityComponentNames[(size_t)Ec]);
	if (Ec == EntityComponent::None)
		componentName = "Empty";

	std::string classIconPath = "@editres/textures/editor-icons/" + componentName + ".png";
	Texture tex = texManager->GetTextureResource(texManager->LoadTextureFromPath(classIconPath, true, false));

	if (tex.Status == Texture::LoadStatus::Failed && tex.ImagePath.find("fallback") == std::string::npos)
	{
		const std::string& fallbackPath = "@editres/textures/editor-icons/fallback.png";
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

// lifetiem of this value is just `recursiveIterateTree`, which does not create any objects
static GameObject* InsertObjectButtonHoveredOver = nullptr;

static void recursiveIterateTree(GameObject* current)
{
	ZoneScopedC(tracy::Color::DarkSeaGreen);

	// https://github.com/ocornut/imgui/issues/581#issuecomment-216054349
	// 07/10/2024
	GameObject* nodeClicked = nullptr;

	if (ExplorerShouldSeekToCurrentSelection && isInSelections(current))
	{
		ExplorerShouldSeekToCurrentSelection = false;
		ImGui::ScrollToItem();
	}

	InsertObjectButtonHoveredOver = nullptr;

	current->ForEachChild([&](GameObject* object)
	{
		ImGui::PushID((int)object->ObjectId);

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

		ReflectorRef primaryComponent = object->Components.size() > 0 ? object->Components[0] : ReflectorRef();
		if (primaryComponent.Type == EntityComponent::Transform && object->Components.size() > 1)
			primaryComponent = object->Components[1];

		if (ExplorerShouldSeekToCurrentSelection && Selections.size() > 0)
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

		bool open = ImGui::TreeNodeEx((const void*)nullptr, flags, "%s", "");

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
		{
			ImGui::SetDragDropPayload("Explorer_DragGameObject", &object->ObjectId, sizeof(uint32_t));
			ImGui::Text("Dragging %s", object->Name.c_str());
			ImGui::EndDragDropSource();
			nodeClicked = object;
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Explorer_DragGameObject"))
			{
				uint32_t child = *(uint32_t*)payload->Data;

				try
				{
					GameObject::GetObjectById(child)->SetParent(object);
				}
				catch (const std::runtime_error& e)
				{
					setErrorMessage(e.what());
				}
			}

			ImGui::EndDragDropTarget();
		}

		nodeClicked = ImGui::IsItemClicked() ? object : nodeClicked;
		openInserter = ImGui::IsItemClicked(ImGuiMouseButton_Right) ? true : openInserter;
		isHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) ? true : isHovered;

		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() - style.IndentSpacing * 0.6f + 1.5f);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.f); // not the faintest idea

		ImGui::ImageWithBg(
			getIconForComponent(primaryComponent.Type).GpuId,
			ImVec2(16.f, 16.f),
			ImVec2(0.f, 0.f),
			ImVec2(1.f, 1.f),
			ImVec4(),
			object->Enabled ? ImVec4(1.f, 1.f, 1.f, 1.f) : ImVec4(.5f, .5f, .5f, 1.f)
		);
		nodeClicked = ImGui::IsItemClicked() ? object : nodeClicked;
		openInserter = ImGui::IsItemClicked(ImGuiMouseButton_Right) ? true : openInserter;
		isHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) ? true : isHovered;

		ImGui::SameLine();
		ImGui::TextUnformatted(object->Name.c_str());

		nodeClicked = ImGui::IsItemClicked() ? object : nodeClicked;
		openInserter = ImGui::IsItemClicked(ImGuiMouseButton_Right) ? true : openInserter;
		isHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) ? true : isHovered;

		VisibleTreeWip.push_back(object);

		if (openInserter || ImGui::IsItemClicked(ImGuiMouseButton_Right))
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
			ImVec2 defaultSize = { defLabelSize.x + style.FramePadding.x * 2.f, defLabelSize.y + style.FramePadding.y * 2.f };

			// make it the same size on both axes
			ImGui::Button("+", ImVec2(defaultSize.y, defaultSize.y));

			// the above call to `::Button` will always
			// return false, ig this does something different
			// with the ordering 15/12/2024
			if (ImGui::IsItemClicked())
			{
				ObjectInsertionTarget = object;
				ImGui::OpenPopup(45);
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetItemTooltip("Insert new Object");
				InsertObjectButtonHoveredOver = object;
			}
		}

		if (open)
		{
			recursiveIterateTree(object);
			ImGui::TreePop();
		}

		ImGui::PopID();
		return true;
	});

	if (nodeClicked)
		onTreeItemClicked(nodeClicked);
}

static bool resetConflictedProperty(const char* /* PropName */, char* Buf = nullptr)
{
	char dbuf[2] = { 0 };
	Buf = Buf ? Buf : dbuf;

	bool reset = ImGui::InputText("##", Buf, 2);
	ImGui::SetItemTooltip("Start typing here to reset this conflicting property to a default");

	return reset;
}

static const ImVec4 ApiPropertyColor = ImVec4(.52f, .69f, 1.f, 1.f);
static const ImVec4 ApiFunctionColor = ImVec4(.91f, .41f, .12f, 1.f);
static const ImVec4 ApiEventColor = ImVec4(1.f, .93f, 0.f, 1.f);
static const ImVec4 ApiTypeColor = ImVec4(.62f, 0.f, .55f, 1.f);

static bool DocumentationViewerOpen = false;
static int DocumentationViewerSection = -1;
static int DocumentationViewerSubPage = 0;
static std::string DocumentationViewerSubPageName = "";

static std::string_view ForceRenderingTooltip = "";

static void propertyTooltip(const std::string_view& PropName, EntityComponent Component, Reflection::ValueType Type)
{
	static std::unordered_map<std::string, std::string> PropTips;

	if (ForceRenderingTooltip.size() > 0 && PropName != ForceRenderingTooltip)
		return;

	if (ImGui::IsItemHovered() || ForceRenderingTooltip.size() > 0)
	{
		std::string PropNameStr = std::string(PropName);

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
				std::string desc;
				if (it.value().is_string())
					desc = it.value();
				else
					desc = it.value().value("Description", "No description");

				std::string tip = addLinebreaks(desc, 75);

				PropTips[PropNameStr] = tip;
				pcit = PropTips.find(PropNameStr);
			}
		}

		std::string fulltip = Reflection::TypeFits(Type, Reflection::ValueType::GameObject) ? "\n(CTRL+LClick to select, CTRL+RClick to set to nil)" : "";
		
		if (pcit != PropTips.end())
			fulltip = pcit->second + fulltip;

		static ImVec2 PrevTooltipPos;

		if (ImGui::IsKeyDown(ImGuiKey_LeftAlt))
		{
			ImGui::SetNextWindowPos(PrevTooltipPos);
			ForceRenderingTooltip = PropName;
		}
		else
			ForceRenderingTooltip = "";

		bool drawContent = false;

		if (ForceRenderingTooltip.size() > 0)
			drawContent = ImGui::Begin(
				"TooltipWindow",
				nullptr,
				ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize
					| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking
			);
		else
			drawContent = ImGui::BeginTooltip();

		if (drawContent)
		{
			if (ImGui::TextLink(PropName.data()))
			{
				DocumentationViewerOpen = true;
				DocumentationViewerSection = 0;
				DocumentationViewerSubPage = (int)Component;
			}

			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Text, ApiTypeColor);
			ImGui::TextUnformatted(Reflection::TypeAsString(Type).c_str());
			ImGui::PopStyleColor();

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.4f, .4f, .4f, 1.f));
			ImGui::TextUnformatted(fulltip.data());
			ImGui::PopStyleColor();
		}

		if (ForceRenderingTooltip.size() == 0)
		{
			if (drawContent)
			{
				PrevTooltipPos = ImGui::GetWindowPos();
				ImGui::EndTooltip();
			}
		}
		else
			ImGui::End();
	}
}

static void renderDescription(const nlohmann::json& DescriptionJson, size_t nCharsPerLine)
{
	if (DescriptionJson.is_string())
	{
		std::string desc = addLinebreaks(DescriptionJson, nCharsPerLine);
		ImGui::Text("* %s", desc.c_str());
	}
	else if (DescriptionJson.is_object())
	{
		renderDescription(DescriptionJson["Description"], nCharsPerLine);
	}
	else if (DescriptionJson.is_array())
	{
		for (auto descit = DescriptionJson.begin(); descit != DescriptionJson.end(); descit++)
		{
			std::string desc = addLinebreaks(descit.value(), nCharsPerLine);
			ImGui::Text("* %s", desc.c_str());
		}
	}
	else if (DescriptionJson.is_null())
		ImGui::Text("No description provided");

	else
		ImGui::Text("Unexpected description type '%s'", DescriptionJson.type_name());
}

static std::string funcArgumentDefsToString(const nlohmann::json& In)
{
	if (In.is_null())
		return "";

	else if (In.is_string())
		return (std::string)In;

	else
	{
		std::string str;

		for (auto it = In.begin(); it != In.end(); it++)
		{
			if (it.value().is_string())
				str.append(it.value());
			else
				str.append(std::format("{} [{}]", (std::string)it.value()[0], (std::string)it.value()[1]));

			str.append(", ");
		}

		str = str.substr(0, str.size() - 2);
		return str;
	}
}

static void renderPropertySignature(const std::string& name, const std::string& type)
{
	ImGui::PushStyleColor(ImGuiCol_Text, ApiPropertyColor);
	ImGui::Text("%s: ", name.c_str());
	ImGui::PopStyleColor();

	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Text, ApiTypeColor);
	ImGui::TextUnformatted(type.c_str());
	ImGui::PopStyleColor();
}

static void renderFunctionSignature(const nlohmann::json::const_iterator& memberIt, const std::string& in, const std::string& out)
{
	ImGui::PushStyleColor(ImGuiCol_Text, ApiFunctionColor);
	ImGui::Text("%s(", memberIt.key().c_str());
	ImGui::PopStyleColor();
	
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Text, ApiTypeColor);
	ImGui::TextUnformatted(funcArgumentDefsToString(in).c_str());
	ImGui::PopStyleColor();

	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Text, ApiFunctionColor);
	ImGui::TextUnformatted(")");
	ImGui::PopStyleColor();

	if (out.size() > 0)
	{
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Text, ApiFunctionColor);
		ImGui::Text(": ");
		ImGui::PopStyleColor();

		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Text, ApiTypeColor);
		ImGui::TextUnformatted(out.c_str());
		ImGui::PopStyleColor();
	}
}

static void renderFunctionSignature(const nlohmann::json::const_iterator& memberIt)
{
	renderFunctionSignature(memberIt, funcArgumentDefsToString(memberIt.value().value("In", nlohmann::json())), memberIt.value().value("Out", ""));
}

// events do not have a signature function, it's done directly inside `renderDocumentationViewer`

static void renderApiMemberBulletpoint(const nlohmann::json::const_iterator& memberIt, const size_t nCharsPerLine)
{
	const nlohmann::json& member = memberIt.value();

	if (const auto& typeIt = member.find("Type"); typeIt != member.end())
		renderPropertySignature(memberIt.key(), typeIt.value());
	else
		renderFunctionSignature(memberIt);

	renderDescription(member.value("Description", nlohmann::json()), nCharsPerLine);
}

static void renderDocumentationViewer()
{
	if (!DocumentationViewerOpen)
		return;

	bool open = true;

	if (!ImGui::Begin("Documentation Viewer", &open))
	{
		ImGui::End();
		DocumentationViewerOpen = open;
		return;
	}

	ImGui::BeginChild(1983, ImGui::GetContentRegionAvail() * ImVec2(0.2f, 1.f), ImGuiChildFlags_Borders);

	if (DocumentationViewerSection == 0)
		ImGui::SetNextItemOpen(true);

	bool sectionOpen = ImGui::TreeNodeEx("Game Object", DocumentationViewerSection == 0 ? ImGuiTreeNodeFlags_Selected : 0);
	if (ImGui::IsItemClicked())
	{
		DocumentationViewerSection = 0;
		DocumentationViewerSubPage = 0;
	}

	if (sectionOpen)
	{
		for (int i = 1; i < (int)EntityComponent::__count; i++)
		{
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf;
			flags |= (DocumentationViewerSection == 0 && DocumentationViewerSubPage == i) ? ImGuiTreeNodeFlags_Selected : 0;

			float startX = ImGui::GetCursorPosX();
			bool popen = ImGui::TreeNodeEx(s_EntityComponentNames[i].data(), flags, "                           ");

			if (ImGui::IsItemClicked())
			{
				DocumentationViewerSection = 0;
				DocumentationViewerSubPage = i;
			}

			ImGui::SameLine();
			ImGui::SetCursorPosX(startX);

			ImGui::Image(
				getIconForComponent((EntityComponent)i).GpuId,
				ImVec2(16, 16)
			);

			if (ImGui::IsItemClicked())
			{
				DocumentationViewerSection = 0;
				DocumentationViewerSubPage = i;
			}

			ImGui::SameLine();

			ImGui::TextUnformatted(s_EntityComponentNames[i].data());

			if (ImGui::IsItemClicked())
			{
				DocumentationViewerSection = 0;
				DocumentationViewerSubPage = i;
			}

			if (popen)
				ImGui::TreePop();
		}

		ImGui::TreePop();
	}

	if (DocumentationViewerSection == 1)
		ImGui::SetNextItemOpen(true);

	sectionOpen = ImGui::TreeNodeEx("Datatypes", DocumentationViewerSection == 1 ? ImGuiTreeNodeFlags_Selected : 0);
	if (ImGui::IsItemClicked())
	{
		DocumentationViewerSection = 1;
		DocumentationViewerSubPageName = "";
	}

	const nlohmann::json& datatypesDoc = DocumentationJson["ScriptEnv"]["Datatypes"];

	if (sectionOpen)
	{
		for (auto it = datatypesDoc.begin(); it != datatypesDoc.end(); it++)
		{
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf;
			flags |= (DocumentationViewerSection == 1 && DocumentationViewerSubPageName == it.key()) ? ImGuiTreeNodeFlags_Selected : 0;

			bool popen = ImGui::TreeNodeEx(it.key().c_str(), flags);

			if (ImGui::IsItemClicked())
			{
				DocumentationViewerSection = 1;
				DocumentationViewerSubPageName = it.key();
			}

			if (popen)
				ImGui::TreePop();
		}

		ImGui::TreePop();
	}

	if (DocumentationViewerOpen)
		ImGui::SetNextItemOpen(true);

	sectionOpen = ImGui::TreeNodeEx("Libraries", DocumentationViewerSection == 2 ? ImGuiTreeNodeFlags_Selected : 0);
	if (ImGui::IsItemClicked())
	{
		DocumentationViewerSection = 2;
		DocumentationViewerSubPageName = "";
	}

	const nlohmann::json& librariesDoc = DocumentationJson["ScriptEnv"]["Libraries"];

	if (sectionOpen)
	{
		for (auto it = librariesDoc.begin(); it != librariesDoc.end(); it++)
		{
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf;
			flags |= (DocumentationViewerSection == 2 && DocumentationViewerSubPageName == it.key()) ? ImGuiTreeNodeFlags_Selected : 0;

			bool popen = ImGui::TreeNodeEx(it.key().c_str(), flags);

			if (ImGui::IsItemClicked())
			{
				DocumentationViewerSection = 2;
				DocumentationViewerSubPageName = it.key();
			}

			if (popen)
				ImGui::TreePop();
		}

		ImGui::TreePop();
	}

	if (DocumentationViewerSection == 3)
		ImGui::SetNextItemOpen(true);

	sectionOpen = ImGui::TreeNodeEx("Globals", ImGuiTreeNodeFlags_Leaf | (DocumentationViewerSection == 3 ? ImGuiTreeNodeFlags_Selected : 0));
	if (ImGui::IsItemClicked())
	{
		DocumentationViewerSection = 3;
		DocumentationViewerSubPage = 0;
	}

	if (sectionOpen)
		ImGui::TreePop();

	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild(67, ImVec2(), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
	ImVec2 space = ImGui::GetContentRegionAvail();
	float charsize = ImGui::CalcTextSize("").y;
	size_t nCharsPerLine = (size_t)std::floor(space.x * 1.65f / charsize);

	switch (DocumentationViewerSection)
	{

	case 0:
	{
		std::string_view name = DocumentationViewerSubPage == 0 ? "GameObject" : s_EntityComponentNames[DocumentationViewerSubPage];

		ImGui::SeparatorText(name.data());

		const nlohmann::json& description = DocumentationViewerSubPage == 0
												? DocumentationJson["ScriptEnv"]["Datatypes"]["GameObject"]["Description"]
												: ObjectDocCommentsJson["Components"][name]["Description"];

		renderDescription(description, nCharsPerLine);

		const nlohmann::json& apiDocs = DocumentationViewerSubPage == 0 ? ObjectDocCommentsJson["Base"] : ObjectDocCommentsJson["Components"][name];
		const nlohmann::json& api = DocumentationViewerSubPage == 0 ? ApiDumpJson["GameObject"]["Base"] : ApiDumpJson["GameObject"]["Components"][name];

		if (const auto& props = api.find("Properties"); props != api.end())
		{
			ImGui::NewLine();
			ImGui::SeparatorText("Properties");

			for (auto ita = props.value().begin(); ita != props.value().end(); ita++)
			{
				renderPropertySignature(ita.key(), ita.value());
				renderDescription(apiDocs["Properties"][ita.key()], nCharsPerLine);
			}
		}

		if (const auto& methods = api.find("Methods"); methods != api.end())
		{
			ImGui::NewLine();
			ImGui::SeparatorText("Methods");

			for (auto ita = methods.value().begin(); ita != methods.value().end(); ita++)
			{
				std::string type = ita.value();
				size_t iut = type.find_first_of("-"); // the `->` in, for example, `(Integer) -> (GameObject)`

				std::string in = type.substr(1, iut - 3);
				std::string out = type.substr(iut + 4, type.size() - (iut + 5));

				renderFunctionSignature(ita, in, out);
				renderDescription(apiDocs["Methods"][ita.key()], nCharsPerLine);
			}
		}

		if (const auto& events = api.find("Events"); events != api.end())
		{
			ImGui::NewLine();
			ImGui::SeparatorText("Events");

			for (auto ita = events.value().begin(); ita != events.value().end(); ita++)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ApiEventColor);
				ImGui::Text("* %s", ita.key().c_str());
				ImGui::PopStyleColor();
				
				ImGui::SameLine();

				ImGui::PushStyleColor(ImGuiCol_Text, ApiTypeColor);
				ImGui::TextUnformatted(((std::string)ita.value()).c_str());
				ImGui::PopStyleColor();

				renderDescription(apiDocs["Events"][ita.key()], nCharsPerLine);
			}
		}

		break;
	}

	case 1:
	{
		if (DocumentationViewerSubPageName == "")
		{
			ImGui::SeparatorText("Datatypes");
			ImGui::TextUnformatted(DatatypesDocPrologue.c_str());

			break;
		}

		const nlohmann::json& datatype = datatypesDoc[DocumentationViewerSubPageName];

		ImGui::SeparatorText(DocumentationViewerSubPageName.c_str());
		renderDescription(datatype["Description"], nCharsPerLine);
		ImGui::NewLine();

		if (const auto& library = datatype.find("Library"); library != datatype.end())
		{
			ImGui::SeparatorText("Library");

			for (auto memberIt = library.value().begin(); memberIt != library.value().end(); memberIt++)
				renderApiMemberBulletpoint(memberIt, nCharsPerLine);
		}

		ImGui::NewLine();

		if (const auto& type = datatype.find("Members"); type != datatype.end())
		{
			ImGui::SeparatorText("Type");

			for (auto memberIt = type.value().begin(); memberIt != type.value().end(); memberIt++)
			{
				if (type.key()[0] == '_' && type.key()[1] == '_')
					continue; // metamethod, handled later

				renderApiMemberBulletpoint(memberIt, nCharsPerLine);
			}
		}

		break;
	}

	case 2:
	{
		if (DocumentationViewerSubPageName == "")
		{
			ImGui::SeparatorText("Libraries");
			ImGui::TextUnformatted(LibrariesDocPrologue.c_str());

			break;
		}

		const nlohmann::json& library = librariesDoc[DocumentationViewerSubPageName];

		ImGui::SeparatorText(DocumentationViewerSubPageName.c_str());
		renderDescription(library["Description"], nCharsPerLine);

		ImGui::NewLine();
		ImGui::SeparatorText("Members");

		for (auto memberIt = library["Members"].begin(); memberIt != library["Members"].end(); memberIt++)
			renderApiMemberBulletpoint(memberIt, nCharsPerLine);

		break;
	}

	case 3:
	{
		const nlohmann::json& globals = DocumentationJson["ScriptEnv"]["Globals"];

		ImGui::SeparatorText("Globals");
		ImGui::TextUnformatted(GlobalsDocPrologue.c_str());

		for (auto memberIt = globals.begin(); memberIt != globals.end(); memberIt++)
		{
			const nlohmann::json& member = memberIt.value();

			if (const auto& dumpedType = ApiDumpJson["ScriptEnv"]["Globals"][memberIt.key()]; dumpedType != "function")
				renderPropertySignature(memberIt.key(), dumpedType);

			else
				renderFunctionSignature(memberIt);

			renderDescription(member["Description"], nCharsPerLine);
		}

		break;
	}
	
	default:
	{
		ImGui::Text("Select a Section and Page from the left");
		break;
	}
	}

	ImGui::EndChild();

	ImGui::End();
}

static void renderExplorer()
{
	ZoneScoped;

	for (size_t i = 0; i < Selections.size(); i++)
	{
		ObjectHandle& handle = Selections[i];
		if (!handle.HasValue() || !handle.Reference.Referred() || handle->IsDestructionPending)
		{
			Selections[i].Clear();
			Selections[i] = Selections[Selections.size() - 1];
			Selections.erase(Selections.end() - 1);
			i--;
		}
	}

	if (EngineJsonConfig["Tool_Explorer"] == false)
		return;

	bool open = true;
	bool render = ImGui::Begin("Explorer", &open);

	if (!open)
		EngineJsonConfig["Tool_Explorer"] = false;

	if (!render)
	{
		ImGui::End();
		return;
	}

	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		ImGui::OpenPopup(1979); // the mimic!!;

	if (ImGui::IsWindowHovered())
	{
		if (ImGui::IsKeyDown(ImGuiKey_F2) && Selections.size() > 0)
		{
			EngineJsonConfig["Tool_Properties"] = true;
			FocusRenameSelection = true;
		}

		if (ImGui::IsKeyDown(ImGuiKey_Delete) && Selections.size() > 0)
		{
			History* history = History::Get();
			std::optional<size_t> id = history->TryBeginAction("Delete by hotkey");

			for (const ObjectHandle& sel : Selections)
				sel->SetParent(nullptr); // We can't actually `::Destroy` it because then we won't be able to Undo it back

			Selections.clear();

			if (id)
				history->FinishAction(id.value());
		}
	}

	if (IsPickingObject)
	{
		ErrorTooltipMessage = "Pick Object\nRight-click to cancel";
		ErrorTooltipTimeRemaining = 0.1f;

		if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
		{
			Selections = PickerTargets;
			ExplorerShouldSeekToCurrentSelection = true;

			IsPickingObject = false;
			PickerTargets.clear();
			PickerTargetPropName.clear();
		}
	}

	VisibleTreeWip.clear();
	recursiveIterateTree(ExplorerRoot);

	VisibleTree = VisibleTreeWip;

	if (ImGui::BeginPopupEx(45, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::SeparatorText("Insert");

		History* history = History::Get();
		std::optional<size_t> began = history->TryBeginAction("InsertObject");
		bool inserted = false;

		if (ImGui::MenuItem("Empty Object"))
		{
			GameObject* newObject = GameObject::Create();
			newObject->SetParent(ObjectInsertionTarget);
			Selections = { newObject };
			inserted = true;
		}
		ImGui::SetItemTooltip("An Object with no Components");

		for (size_t i = 0; i < std::size(AddableComponents); i++)
		{
			std::string_view name = AddableComponents[i];

			if (ImGui::MenuItem(name.data()))
			{
				GameObject* newObject = nullptr;

				newObject = GameObject::Create(name);

				if (name == "Mesh")
					newObject->AddComponent(EntityComponent::RigidBody);

				newObject->SetParent(ObjectInsertionTarget);
				Selections = { newObject };
			
				ExplorerShouldSeekToCurrentSelection = true;
				ObjectInsertionTarget.Clear();

				if (began)
					inserted = true;
			}
			std::string tooltip = getDescriptionAsString(
				ObjectDocCommentsJson.value("Components", nlohmann::json::object()).value(name, nlohmann::json::object())["Description"],
				32
			);
			if (tooltip.size() > 1)
				ImGui::SetItemTooltip("%s", tooltip.c_str());
		}

		if (began)
		{
			if (inserted)
				history->FinishAction(began.value());
			else
				history->DiscardAction(began.value());
		}

		InsertObjectButtonHoveredOver = nullptr;

		ImGui::EndPopup();
	}
	else
		ObjectInsertionTarget.Clear();

	if (ImGui::BeginPopupEx(1979, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings))
	{
		for (ContextMenuAction action : ContextActionsForSelection)
		{
			if (action == ContextMenuAction::__sectionSeparator)
				ImGui::Separator();

			else
			{
				const char* name = ContextMenuActionStrings[(uint8_t)action];

				if ((Selections.size() > 0 || !ContextMenuActionRequiresSelection[(uint8_t)action]) &&  ImGui::MenuItem(name))
				{
					History* history = History::Get();
					std::optional<size_t> id = action != ContextMenuAction::Rename ? history->TryBeginAction(name) : std::nullopt;

					ContextMenuActionHandlers[(uint8_t)action]();

					if (id)
						history->FinishAction(id.value());
				}
			}
		}

		if (ImGui::BeginMenu("Insert service"))
		{
			for (const std::string_view& serv : s_DataModelServices)
			{
				EntityComponent ec = FindComponentTypeByName(serv);

				if (!ExplorerRoot->FindChildWithComponent(ec) && ImGui::MenuItem(serv.data()))
				{
					GameObject* newServ = GameObject::Create(ec);
					newServ->SetParent(ExplorerRoot);
					Selections = { newServ };
				}
			}

			ImGui::EndMenu();
		}

		ImGui::EndPopup();
	}

	ImGui::End();
}

struct FilesystemNode // Either a file or a directory
{
	std::map<std::string, FilesystemNode> DirectoryContents;
	std::filesystem::path Path;
	std::string Name;
	bool IsDirectory = false;
};

static double FilesViewLastRefreshed = 0.0;
static FilesystemNode FilesViewerRoot = { .Name = "scripts", .IsDirectory = true };
static std::string FilesViewerRootPath = "scripts";
static std::filesystem::path RenameTarget;
static std::string RenameNewValue;
static bool RenameFocusFirstFrame = true;

static void refreshFilesystemNode(FilesystemNode& Node)
{
	ZoneScoped;
	Node.DirectoryContents.clear();

	std::error_code ec;
	for (const auto& it : std::filesystem::directory_iterator(Node.Path, ec))
	{
		if (std::filesystem::is_regular_file(it.path()))
		{
			if (it.path().filename().string() == Node.Name)
				continue; // not sure

			Node.DirectoryContents[it.path().filename().string()] = { .Path = FileRW::MakePathCwdRelative(it.path().string()), .Name = it.path().filename().string(), .IsDirectory = false };
		}
		else if (std::filesystem::is_directory(it.path()))
		{
			FilesystemNode newNode = {
				.Path = it.path(),
				.Name = it.path().filename().string(),
				.IsDirectory = true,
			};

			refreshFilesystemNode(newNode);

			Node.DirectoryContents[it.path().filename().string()] = newNode;
		}
	}

	if (ec)
	{
		setErrorMessage(ec.message());
		FilesViewerRoot = FilesystemNode{ .Path = "scripts/", .Name = "Scripts" };
	}
}

static void createFileOrDirectory(FilesystemNode Parent, bool Directory)
{
	int index = 1;

	while (true)
	{
		std::filesystem::path path = Parent.Path / std::vformat(Directory ? "Folder{}" : "Script{}.luau", std::make_format_args(index));
		if (!(Directory ? std::filesystem::is_directory(path) : std::filesystem::is_regular_file(path)))
		{
			std::string errorMessage;
			bool success = true;

			if (Directory)
			{
				std::error_code ec;
				std::filesystem::create_directory(path, ec);

				if (ec)
				{
					success = false;
					errorMessage = ec.message();
				}
			}
			else
			{
				success = FileRW::WriteFile(path.string(), "print(\"Hello, World!\")\n", &errorMessage);
			}

			if (!success)
				setErrorMessage(std::format("Failed to create '{}': {}", path.string(), errorMessage));

			else
			{
				RenameTarget = path;
				RenameNewValue = path.filename().string();
				RenameNewValue.reserve(32);
				RenameFocusFirstFrame = true;
			}

			break;
		}
		else
			index++;

		if (index > 99)
		{
			setErrorMessage("Failed to create");
			break;
		}
	}

	FilesViewLastRefreshed = 0.0;
}

static void recursiveRenderFilesystemNode(FilesystemNode& Node)
{
	TextureManager* texManager = TextureManager::Get();
	static uint32_t ScriptIconResource = texManager->LoadTextureFromPath("./resources/textures/editor-icons/Script.png", false, false);
	static uint32_t FolderOpenIconResource = texManager->LoadTextureFromPath("./resources/textures/editor-icons/Folder_Open.png", false, false);
	static uint32_t FolderClosedIconResource = texManager->LoadTextureFromPath("./resources/textures/editor-icons/Folder_Closed.png", false, false);

	ImGui::PushID(Node.Path.string().c_str());

	const ImGuiStyle& style = ImGui::GetStyle();

	static std::filesystem::path ContextMenuTarget;

	ImGuiTreeNodeFlags flags = 0
		| ImGuiTreeNodeFlags_OpenOnArrow
		| ImGuiTreeNodeFlags_AllowOverlap
		| ImGuiTreeNodeFlags_SpanAvailWidth
		| ImGuiTreeNodeFlags_DrawLinesFull
		| ImGuiTreeNodeFlags_FramePadding;
	flags |= !Node.IsDirectory ? ImGuiTreeNodeFlags_Leaf : 0;

	if (ContextMenuTarget == Node.Path || RenameTarget == Node.Path)
		flags |= ImGuiTreeNodeFlags_Selected;

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.f, 4.f));

	std::string displayedNodeName = RenameTarget != Node.Path ? Node.Name : " ";

	if (RenameTarget.parent_path() == Node.Path)
		ImGui::SetNextItemOpen(true);

	bool open = ImGui::TreeNodeEx((const void*)nullptr, flags, "   %s", displayedNodeName.c_str());
	bool openScript = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered();
	bool hovered = ImGui::IsItemHovered();

	ImGui::PopStyleVar();

	ImGui::SameLine();
	ImVec2 afterNodePos = ImGui::GetCursorPos();

	if (RenameTarget == Node.Path)
	{
		openScript = false;
		hovered = false;

		ImGui::SetCursorPosX(afterNodePos.x - ImGui::CalcTextSize("  ").x);

		if (RenameFocusFirstFrame)
			ImGui::SetKeyboardFocusHere();

		ImGui::InputText("##", &RenameNewValue);

		if (!ImGui::IsItemActive() && !RenameFocusFirstFrame)
		{
			std::filesystem::path renamed = Node.Path.parent_path() / RenameNewValue;

			if (renamed.filename() != Node.Path.filename() && std::filesystem::is_regular_file(renamed))
				setErrorMessage("A file already exists in the folder with that name");

			else
			{
				std::error_code ec;
				std::filesystem::rename(Node.Path, renamed, ec);

				if (ec)
					setErrorMessage(ec.message());
			}

			RenameTarget.clear();
			FilesViewLastRefreshed = 0.0;
		}

		RenameFocusFirstFrame = false;
		ImGui::SetCursorPos(afterNodePos);
	}

	ImGui::SetCursorPosX(ImGui::GetCursorPosX() - ImGui::CalcTextSize((" " + displayedNodeName).c_str()).x - style.IndentSpacing - 4.f);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.f); // not the faintest idea

	ImGui::Image(
		texManager->GetTextureResource(Node.IsDirectory ? (open ? FolderOpenIconResource : FolderClosedIconResource) : ScriptIconResource).GpuId,
		ImVec2(18.f, 16.f)
	);
	openScript = openScript || (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered());
	hovered = hovered || ImGui::IsItemHovered();

	if (hovered && ImGui::IsMouseDown(ImGuiMouseButton_Right))
		ImGui::OpenPopup("FileContextMenu");

	if (ImGui::BeginPopup("FileContextMenu"))
	{
		ContextMenuTarget = Node.Path;

		if (ImGui::MenuItem("Rename"))
		{
			RenameTarget = Node.Path;
			RenameNewValue = Node.Name;
			RenameNewValue.reserve(32);
			RenameFocusFirstFrame = true;
		}

		if (Node.IsDirectory)
		{
			if (ImGui::MenuItem("Focus"))
			{
				FilesViewerRootPath = Node.Path.string();
				FilesViewLastRefreshed = 0.0;
			}
		}

		if (ImGui::MenuItem("Delete"))
		{
			std::error_code ec;
			std::filesystem::remove_all(Node.Path, ec);

			if (ec)
				setErrorMessage(ec.message());

			FilesViewLastRefreshed = 0.0;
		}

		ImGui::EndPopup();
	}

	if (Node.Path == ContextMenuTarget && !ImGui::IsPopupOpen("FileContextMenu"))
		ContextMenuTarget.clear();

	if (!Node.IsDirectory && openScript && ImGui::IsWindowFocused())
		invokeTextEditor(Node.Path.string());

	ImGui::SetCursorPos(afterNodePos);

	if (Node.IsDirectory && hovered)
	{
		if (ImGui::Button("+", ImVec2(16.f, 16.f)) || (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)))
		{
			ImGui::OpenPopup("CreateFileOrDirectory");
			ContextMenuTarget = Node.Path;
		}
	}
	else
		ImGui::Dummy({ 16.f, 16.f });

	bool wasCreatorOpen = false;

	if (ImGui::BeginPopup("CreateFileOrDirectory"))
	{
		wasCreatorOpen = true;

		if (ImGui::MenuItem("File"))
		{
			createFileOrDirectory(Node, false);
			ContextMenuTarget.clear();
		}
		if (ImGui::MenuItem("Folder"))
		{
			createFileOrDirectory(Node, true);
			ContextMenuTarget.clear();
		}

		ImGui::EndPopup();
	}

	if (wasCreatorOpen && !ImGui::IsPopupOpen("CreateFileOrDirectory"))
		ContextMenuTarget.clear();

	if (open)
	{
		if (Node.IsDirectory)
		{
			for (auto& it : Node.DirectoryContents)
				recursiveRenderFilesystemNode(it.second);
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}

static void renderFilesViewer()
{
	if (EngineJsonConfig["Tool_ScriptExplorer"] == false)
		return;

	std::string name = FilesViewerRoot.Name;
	name[0] = toupper(name[0]);

	bool open = true;
	bool render = ImGui::Begin(std::format("{}###FilesViewer", name).c_str(), &open);

	if (!open)
		EngineJsonConfig["Tool_ScriptExplorer"] = false;

	if (!render)
	{
		ImGui::End();
		return;
	}

	static std::string PreviousNonqualifiedRoot;
	if (std::string newRoot = FileRW::MakePathCwdRelative("dummy"); newRoot != PreviousNonqualifiedRoot)
	{
		FilesViewLastRefreshed = 0.0;
		FilesViewerRoot = FilesystemNode{ .Path = "scripts/", .Name = "Scripts" };
		PreviousNonqualifiedRoot = newRoot;
	}

	if (GetRunningTime() - FilesViewLastRefreshed > 2.f)
	{
		FilesViewerRoot.Path = std::filesystem::path(FileRW::MakePathCwdRelative(FilesViewerRootPath));
		FilesViewerRoot.Name = FilesViewerRoot.Path.filename().string();
		refreshFilesystemNode(FilesViewerRoot);
		FilesViewLastRefreshed = GetRunningTime();
	}

	for (auto& nodeIt : FilesViewerRoot.DirectoryContents)
		recursiveRenderFilesystemNode(nodeIt.second);

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemFocused() && !ImGui::IsAnyItemHovered())
		ImGui::OpenPopup("ViewerContextMenu");

	if (ImGui::BeginPopup("ViewerContextMenu"))
	{
		if (ImGui::BeginMenu("New"))
		{
			if (ImGui::MenuItem("File"))
				createFileOrDirectory(FilesViewerRoot, false);

			if (ImGui::MenuItem("Folder"))
				createFileOrDirectory(FilesViewerRoot, true);

			ImGui::EndMenu();
		}

		if (ImGui::MenuItem("Focus parent"))
		{
			FilesViewerRootPath = std::filesystem::absolute(FileRW::MakePathCwdRelative(FilesViewerRootPath)).parent_path().string();
			FilesViewLastRefreshed = 0.0;
		}

		ImGui::EndPopup();
	}

	ImGui::End();
}

static std::vector<ObjectHandle> PrevEditSelections;

static void renderProperties()
{
	ZoneScoped;

	if (EngineJsonConfig["Tool_Properties"] == false)
		return;

	bool open = true;
	bool render = ImGui::Begin("Properties", &open, ImGuiWindowFlags_HorizontalScrollbar);

	if (!open)
		EngineJsonConfig["Tool_Properties"] = false;

	if (!render)
	{
		ImGui::End();
		return;
	}

	if (Selections.size() > 0)
	{
		if (ImGui::IsWindowHovered() && ImGui::IsKeyDown(ImGuiKey_F2))
			FocusRenameSelection = true;

		uint32_t idSum = 0;
		for (const ObjectHandle& sel : Selections)
			idSum += sel->ObjectId;

		ImGui::PushID(idSum);

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

		static EntityComponent RemoveComponentPopupTarget = EntityComponent::None;

		for (EntityComponent ec : components)
		{
			Texture tex = getIconForComponent(ec);
			ImGui::Image(tex.GpuId, ImVec2(16, 16));
			ImGui::SetItemTooltip(
				"%s\n%s",
				s_EntityComponentNames[(size_t)ec].data(),
				getDescriptionForComponent(ec).c_str()
			);

			if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
			{
				RemoveComponentPopupTarget = ec;
				ImGui::OpenPopup("RemoveComponent");
			}
			else if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			{
				DocumentationViewerOpen = true;
				DocumentationViewerSection = 0;
				DocumentationViewerSubPage = (int)ec;
			}

			ImGui::SameLine();
		}

		if (ImGui::Button("+", ImVec2(16, 16)))
			ImGui::OpenPopup("AddComponent");

		if (ImGui::BeginPopup("AddComponent"))
		{
			for (const std::string_view& comp : AddableComponents)
			{
				if (ImGui::MenuItem(comp.data()))
				{
					EntityComponent ec = FindComponentTypeByName(comp);
					assert(ec != EntityComponent::None);

					for (const ObjectHandle& obj : Selections)
					{
						if (!obj->FindComponentByType(ec))
							obj->AddComponent(ec);
					}
				}

				std::string tooltip = getDescriptionAsString(
					ObjectDocCommentsJson.value("Components", nlohmann::json::object()).value(comp, nlohmann::json::object())["Description"],
					32
				);
				if (tooltip.size() > 1)
					ImGui::SetItemTooltip("%s", tooltip.c_str());
			}

			ImGui::EndPopup();
		}

		if (ImGui::BeginPopup("RemoveComponent"))
		{
			ImGui::SeparatorText(s_EntityComponentNames[(size_t)RemoveComponentPopupTarget].data());

			if (ImGui::MenuItem("Info"))
			{
				DocumentationViewerOpen = true;
				DocumentationViewerSection = 0;
				DocumentationViewerSubPage = (int)RemoveComponentPopupTarget;
			}

			if (ImGui::MenuItem("Remove"))
			{
				for (const ObjectHandle& obj : Selections)
					if (obj->FindComponentByType(RemoveComponentPopupTarget))
						obj->RemoveComponent(RemoveComponentPopupTarget);
			}
			
			ImGui::EndPopup();
		}

		ImGui::PopID();

		std::unordered_map<std::string_view, std::pair<const Reflection::PropertyDescriptor*, Reflection::GenericValue>> props;
		std::unordered_map<std::string_view, bool> conflictingProps;
		std::unordered_map<std::string_view, EntityComponent> propToComponent;
		std::vector<std::string_view> tags;

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
				else
					propToComponent[pname] = sel->MemberToComponentMap[pname].Type;
			}

			for (uint16_t tag : sel->Tags)
				tags.push_back(GameObject::s_Collections[tag].Name);
		}

		using PropsIteratorType = std::pair<std::string_view, std::pair<const Reflection::PropertyDescriptor*, Reflection::GenericValue>>;

		std::vector<PropsIteratorType> propsOrdered = std::vector<PropsIteratorType>(props.begin(), props.end());
		std::sort(propsOrdered.begin(), propsOrdered.end(), [](const auto& a, const auto& b)
			{
				return a.first < b.first;
			});
		std::sort(tags.begin(), tags.end());

		float cavailX = ImGui::GetContentRegionAvail().x;
		float halfWidth = cavailX / 2.f;

		ImGui::Separator();

		for (size_t propIndex = 0; propIndex < propsOrdered.size(); propIndex++)
		{
			const PropsIteratorType& propIt = propsOrdered[propIndex];
			const std::pair<const Reflection::PropertyDescriptor*, Reflection::GenericValue> propItem = propIt.second;

			const std::string_view& propName = propIt.first;
			const Reflection::PropertyDescriptor* propDesc = propItem.first;
			const Reflection::GenericValue& curVal = propItem.second;

			ImGui::PushID(propItem.first);

			const char* propNameCStr = propName.data();

			bool doConflict = conflictingProps[propName];

			if (!propDesc->Set)
			{
				// no setter (read-only property, such as Class or ObjectId)
				// 07/07/2024

				if (doConflict)
				{
					ImGui::TextUnformatted(propNameCStr);
					ImGui::SameLine();
					propertyTooltip(propName, propToComponent[propName], propDesc->Type);

					ImGui::SetCursorPosX(halfWidth);
					ImGui::TextUnformatted("<DIFFERENT>");
				}
				else
				{
					std::string curValStr = curVal.ToString();

					if (curVal.Type == Reflection::ValueType::Matrix)
					{
						ImGui::Text("%s: ", propNameCStr);

						ImGui::Indent();

						curValStr.insert(curValStr.begin() + curValStr.find_first_of("Ang"), '\n');
						ImGui::TextUnformatted(curValStr.c_str());

						ImGui::Unindent();

						if (ImGui::BeginItemTooltip())
						{
							ImGui::TextUnformatted(curValStr.c_str());
							ImGui::EndTooltip();
						}
					}
					else
					{
						ImGui::TextUnformatted(propNameCStr);
						ImGui::SameLine();
						propertyTooltip(propName, propToComponent[propName], propDesc->Type);

						ImGui::SetCursorPosX(halfWidth);
						ImGui::TextUnformatted(curValStr.c_str());
					}
				}

				ImGui::Separator();
				ImGui::PopID();
				continue;
			}

			Reflection::GenericValue newVal = curVal;
			bool canChangeValue = true;
			bool valueWasEditedManual = false;
			bool deactivatedAfterEdit = false;

			ImGui::TextUnformatted(propNameCStr);
			ImGui::SameLine();

			ImVec2 cursorStart = ImGui::GetCursorPos();
			ImGui::SetCursorPosX(halfWidth);

			if (!doConflict)
				propertyTooltip(propName, propToComponent[propName], propDesc->Type);

			switch (propDesc->Type & ~Reflection::ValueType::Null)
			{

			case Reflection::ValueType::String:
			{
				std::string_view str = curVal.AsStringView();

				const uint32_t INPUT_TEXT_BUFFER_ADDITIONAL = 64;
				uint32_t allocSize = (uint32_t)str.size() + INPUT_TEXT_BUFFER_ADDITIONAL;

				char* buf = BufferInitialize(
					allocSize,
					"<Initial Value 29/09/2024 Hey guys How we doing today>"
				);
				CopyStringToBuffer(buf, allocSize, doConflict ? "" : str);

				bool focused = false;
				if (FocusRenameSelection && propName == "Name")
				{
					FocusRenameSelection = false;
					ImGui::SetKeyboardFocusHere();
					focused = true;
				}

				ImGui::SetCursorPosX(halfWidth);
				ImGui::InputText("##", buf, allocSize);

				if (focused)
					ImGui::SetScrollX(0.f);

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
					ImGui::Checkbox("##", &newVal.Val.Bool);

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
					ImGui::InputDouble("##", &newVal.Val.Double);

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
					ImGui::InputInt("##", &i);
					newVal.Val.Int = i;
				}

				break;
			}

			case Reflection::ValueType::GameObject:
			{
				if (doConflict)
				{
					bool pick = ImGui::Button(" ");

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

					ImGui::InputText("##", &str);

					if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
					{
						if (ImGui::GetIO().KeyCtrl)
						{
							if (referenced)
							{
								GameObject* target = GameObject::FromGenericValue(newVal);
								Selections = { target };
								ExplorerShouldSeekToCurrentSelection = true;
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
					ImGui::ColorEdit3("##", &newVal.Val.Vec3.x);

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
					ImGui::InputFloat2("##", &newVal.Val.Vec2.x);

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
					ImGui::InputFloat3("##", &newVal.Val.Vec3.x);

				break;
			}

			case Reflection::ValueType::Matrix:
			{
				if (!doConflict)
				{
					ImGui::SetCursorPos(cursorStart);
					ImGui::NewLine(); // need to put this on a new line to prevent UI overlapping

					glm::mat4 mat = curVal.AsMatrix();

					float pos[3] =
					{
						mat[3][0],
						mat[3][1],
						mat[3][2]
					};

					// PLEASE GOD JUST WORK ALREADY
					// 21/09/2024
					glm::vec3 rotrads{};

					glm::extractEulerAngleYXZ(mat, rotrads.y, rotrads.x, rotrads.z);

					//mat = glm::rotate(mat, -rotrads[0], glm::vec3(1.f, 0.f, 0.f));
					//mat = glm::rotate(mat, -rotrads[1], glm::vec3(0.f, 1.f, 0.f));
					//mat = glm::rotate(mat, -rotrads[2], glm::vec3(0.f, 0.f, 1.f));

					float rotdegs[3] =
					{
						glm::degrees(rotrads.x),
						glm::degrees(rotrads.y),
						glm::degrees(rotrads.z)
					};

					if (pos[0] != pos[0] || pos[1] != pos[1] || pos[2] != pos[2] || rotdegs[0] != rotdegs[0] || rotdegs[1] != rotdegs[1] || rotdegs[2] != rotdegs[2])
					{
						newVal = glm::mat4(1.f);
						valueWasEditedManual = true;
						break;
					}

					ImGui::Indent();

					ImGui::InputFloat3("Position", pos);
					valueWasEditedManual = ImGui::IsItemEdited();
					deactivatedAfterEdit = ImGui::IsItemDeactivatedAfterEdit();

					ImGui::InputFloat3("Rotation", rotdegs);
					ImGui::Unindent();

					mat = glm::mat4(1.f);

					mat[3][0] = pos[0];
					mat[3][1] = pos[1];
					mat[3][2] = pos[2];

					mat *= glm::eulerAngleYXZ(glm::radians(rotdegs[1]), glm::radians(rotdegs[0]), glm::radians(rotdegs[2]));

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

			static std::optional<size_t> BeganPropertyEditingAction;

			static std::string PrevEditPropName;
			static Reflection::GenericValue PrevEditNewValue;
			const char* const EditPropertyTempName = "EditProperty_TEMP";
			History* history = History::Get();

			if ((ImGui::IsItemEdited() || valueWasEditedManual) && canChangeValue)
			{
				// Awful and hacky
				PrevEditSelections = Selections;
				PrevEditPropName = propName;
				PrevEditNewValue = newVal;

				if (history->CanUndo() && history->GetActionHistory().back().Name == EditPropertyTempName)
					history->Undo();

				BeganPropertyEditingAction = history->TryBeginAction(EditPropertyTempName);

				try
				{
					for (const ObjectHandle& sel : Selections)
						sel->SetPropertyValue(propName, newVal);
				}
				catch (const std::runtime_error& Err)
				{
					setErrorMessage(Err.what());
				}

				if (BeganPropertyEditingAction)
					history->FinishAction(BeganPropertyEditingAction.value());
			}

			if (deactivatedAfterEdit || ImGui::IsItemDeactivatedAfterEdit())
			{
				if (history->CanUndo() && history->GetActionHistory().back().Name == EditPropertyTempName)
				{
					history->Undo();

					if (std::optional<size_t> began = history->TryBeginAction("EditProperty"))
					{
						try
						{
							for (const ObjectHandle& sel : PrevEditSelections)
								sel->SetPropertyValue(PrevEditPropName, PrevEditNewValue);
						}
						catch (const std::runtime_error& Err)
						{
							setErrorMessage(Err.what());
						}

						if (history->GetCurrentAction()->Events.size() == 0)
							history->DiscardAction(began.value()); // started typing, but didn't actually change the value in the end
						else
							history->FinishAction(began.value());
					}
					else
					{
						setErrorMessage(std::format(
							"Could not begin History Action to re-apply properties after you finished interacting with {}{}",
							PrevEditPropName, history->GetCurrentAction().has_value() ? std::format(". Blocked by the {} Action", history->GetCurrentAction()->Name) : ""
						));
					}
				}
			}

			ImGui::Separator();

			ImGui::PopID();
		}

		ImGui::TextUnformatted("Tags");
		ImGui::SameLine();
		ImGui::SetCursorPosX(halfWidth);

		for (const std::string_view& tag : tags)
		{
			ImGui::SetCursorPosX(halfWidth);
			ImGui::PushID(tag.data());

			if (ImGui::Button("X", ImVec2(16.f, 16.f)))
			{
				for (const ObjectHandle& sel : Selections)
					sel->RemoveTag(std::string(tag));
			}

			ImGui::PopID();
			ImGui::SameLine();
			ImGui::TextUnformatted(tag.data());
		}

		ImGui::SetCursorPosX(halfWidth);
		if (ImGui::Button("+"))
			ImGui::OpenPopup("AddTags");

		if (ImGui::BeginPopup("AddTags"))
		{
			static char EntryBuffer[64] = { 0 };
			ImGui::InputText("##", EntryBuffer, 64);

			if (ImGui::IsItemDeactivatedAfterEdit() && strlen(EntryBuffer) > 0)
			{
				for (const ObjectHandle& sel : Selections)
					sel->AddTag(std::string(EntryBuffer));

				ImGui::CloseCurrentPopup();
			}

			std::vector<std::string_view> collectionNames;
			collectionNames.reserve(GameObject::s_Collections.size());
			for (const GameObject::Collection& collection : GameObject::s_Collections)
				collectionNames.emplace_back(collection.Name);

			std::sort(collectionNames.begin(), collectionNames.end());

			for (const std::string_view& name : collectionNames)
			{
				if (ImGui::MenuItem(name.data()))
				{
					for (const ObjectHandle& sel : Selections)
						sel->AddTag(std::string(name));

					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::EndPopup();
		}

		ImGui::PopID();
	}

	ImGui::End();
}

void InlayEditor::UpdateAndRender(double DeltaTime)
{
	ZoneScopedC(tracy::Color::DarkSeaGreen);

	if (!ExplorerRoot.Reference.Referred())
		ExplorerRoot = GameObject::GetObjectById(GameObject::s_DataModel);

	ErrorTooltipTimeRemaining -= DeltaTime;

	if (ErrorTooltipTimeRemaining > 0.f)
		ImGui::SetTooltip("%s", ErrorTooltipMessage.c_str());

	renderTextEditors();
	renderShaderPipelinesEditor();
	renderMaterialEditor();
	renderExplorer();
	renderFilesViewer();
	renderProperties();
	renderDocumentationViewer();
}

void InlayEditor::SetExplorerRoot(const ObjectHandle NewRoot)
{
	ExplorerRoot = NewRoot;
}

void InlayEditor::SetExplorerSelections(const std::vector<ObjectHandle>& NewSelections)
{
	Selections = NewSelections;
	ExplorerShouldSeekToCurrentSelection = true;
}

const std::vector<ObjectHandle>& InlayEditor::GetExplorerSelections()
{
	for (size_t i = 0; i < Selections.size(); i++)
	{
		ObjectHandle& handle = Selections[i];
		if (!handle.HasValue() || !handle.Reference.Referred() || handle->IsDestructionPending)
		{
			Selections[i].Clear();
			Selections[i] = Selections[Selections.size() - 1];
			Selections.erase(Selections.end() - 1);
			i--;
		}
	}

	return Selections;
}

void InlayEditor::OpenTextDocument(const std::string& Path, int Line)
{
	TextEditorTab& tab = invokeTextEditor(Path);
	tab.JumpToLine = Line;
}

void InlayEditor::Shutdown()
{
	MtlPreviewCamera.Clear();
	ExplorerRoot.Clear();
	LastSelected.Clear();
	ObjectInsertionTarget.Clear();
	PrevEditSelections.clear();
	Selections.clear();
	VisibleTree.clear();
	VisibleTreeWip.clear();
	PickerTargets.clear();
	
	for (TextEditorTab& tab : s_TextEditors)
	{
		textEditorSaveFile(tab);
		if (tab.FileStream)
		{
			tab.FileStream->close();
			delete tab.FileStream;
			tab.FileStream = nullptr;
		}
	}

	s_TextEditors.clear();
}

#include <imgui/backends/imgui_impl_opengl3.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <thread>
#include <cmath>

#include "Engine.hpp"

struct LuauStatusDisplayInfo
{
	const char* Id = nullptr;
	const char* Description = nullptr;
	ImVec4 Color;
};

struct LuauCoroutineStatusDisplayInfo
{
	const char* Id = nullptr;
	const char* Description = nullptr;
	ImVec4 Color;
};

const LuauStatusDisplayInfo LuauStatuses[] = {
	{ "OK",   "OK\nOK",                      ImVec4(0.f, 1.f, 0.f, 1.f) },
	{ "YD",   "YD\nYielded",                 ImVec4(1.f, .7f, 0.f, 1.f) },
	{ "ER",   "ER\nError while running",     ImVec4(1.f, 0.f, 0.f, 1.f) },
	{ "ES",   "ES\nSyntax error (legacy)",   ImVec4(1.f, 0.f, 0.f, 1.f) },
	{ "EM",   "EM\nMemory error",            ImVec4(1.f, 0.f, 0.f, 1.f) },
	{ "EE",   "EE\nUnknown error",           ImVec4(1.f, 0.f, 0.f, 1.f) },
	{ "BR",   "BR\nBroke into debugger",     ImVec4(.8f, .4f, 0.f, 1.f) }
};

// descriptions courtesy of `luau/VM/include/lua.h`
const LuauCoroutineStatusDisplayInfo LuauCoroutineStatuses[] = {
	{ "RUN",   "RUN\nRunning",                                   ImVec4(0.f, 0.f, 1.f, 1.f) },
	{ "SUS",   "SUS\nSuspended",                                 ImVec4(1.f, .7f, 0.f, 1.f) },
	{ "NOR",   "NOR\n'Normal' (it resumed another coroutine)",   ImVec4(0.f, 1.f, 0.f, 1.f) },
	{ "FIN",   "FIN\nFinished",                                  ImVec4(0.f, .5f, 0.f, 1.f) },
	{ "ERR",   "ERR\nFinished with error",                       ImVec4(1.f, 0.f, 0.f, 1.f) }
};

static bool debugVariable(lua_State* L, bool CanEdit = true)
{
	ZoneScoped;
	luaL_checkstack(L, 2, "debugVariable");

	std::string varname;
	switch (lua_type(L, -2))
	{
	case LUA_TBOOLEAN: case LUA_TNUMBER: case LUA_TVECTOR:
	{
		varname = std::format("[{}]", luaL_tolstring(L, -2, nullptr));
		lua_pop(L, 1);
		break;
	}
	case LUA_TSTRING:
	{
		varname = lua_tostring(L, -2);
		break;
	}
	default:
	{
		const char* ktn = luaL_typename(L, -2);
		varname = std::format("[{} ({})]", luaL_tolstring(L, -2, nullptr), ktn);
		lua_pop(L, 1);
	}
	}

	const char* vtn = luaL_typename(L, -1);
	constexpr ImGuiTreeNodeFlags NodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DrawLinesFull;
	bool changed = false;

	switch (lua_type(L, -1))
	{
	case LUA_TFUNCTION:
	{
		lua_Debug ar = {};
		lua_getinfo(L, -1, "sluan", &ar);
		ar.name = ar.name ? ar.name : "<anonymous>";

		std::string reassigned = ar.name != varname ? std::format("{} ", ar.name) : "";

		if (ImGui::TreeNodeEx(varname.c_str(), NodeFlags, "%s: %s(function)", varname.c_str(), reassigned.c_str()))
		{
			ImGui::Text("%s function '%s'", ar.what[0] == 'C' ? "C" : "Luau", ar.name ? ar.name : "<anonymous>");

			if (ar.linedefined != -1 || strcmp(ar.short_src, "[C]") != 0 || !ar.isvararg || ar.nupvals != 0)
			{
				ImGui::Text("Defined %s:%i", ar.short_src, ar.linedefined);

				if (ar.isvararg)
					ImGui::TextUnformatted("# Parameters: Variadic");
				else
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

			int tableIndex = lua_gettop(L);
			bool isReadOnly = lua_getreadonly(L, -1);

			lua_pushnil(L);
			while (lua_next(L, -2))
			{
				if (debugVariable(L, !isReadOnly))
				{
					lua_pushvalue(L, -2);
					lua_pushvalue(L, -2);
					lua_settable(L, tableIndex);
				}
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
		std::string valueString;
		bool editableValue = false;

		switch (lua_type(L, -1))
		{
		case LUA_TNIL: // for Stack view only
		{
			valueString = "nil";
			break;
		}
		case LUA_TBOOLEAN: case LUA_TNUMBER: case LUA_TSTRING: case LUA_TVECTOR:
		{
			if (!CanEdit)
			{
				switch (lua_type(L, -1))
				{
				case LUA_TBOOLEAN: case LUA_TNUMBER: case LUA_TVECTOR:
				{
					valueString = luaL_tolstring(L, -1, nullptr);
					lua_pop(L, 1);
					break;
				}
				default:
				{
					assert(lua_type(L, -1) == LUA_TSTRING);
					valueString = std::format("\"{}\"", lua_tostring(L, -1));
				}
				}
			}
			else
			{
				editableValue = true;
			}

			break;
		}
		default:
		{
			valueString = std::format("{} ({})", luaL_tolstring(L, -1, nullptr), vtn);
			lua_pop(L, 1);
		}
		}

		ImGui::PushID(varname.c_str());

		if (ImGui::TreeNodeEx(
			varname.c_str(),
			NodeFlags | ImGuiTreeNodeFlags_Leaf,
			"%s: %s",
			varname.c_str(), valueString.c_str()
		))
			ImGui::TreePop();

		if (editableValue)
		{
			ImGui::SameLine();

			switch (lua_type(L, -1))
			{
			case LUA_TBOOLEAN:
			{
				bool value = lua_toboolean(L, -1);

				if (ImGui::Checkbox("##", &value))
				{
					lua_pop(L, 1);
					lua_pushboolean(L, value);
					changed = true;
				}
				break;
			}
			case LUA_TNUMBER:
			{
				double value = lua_tonumber(L, -1);

				if (ImGui::InputDouble("##", &value))
				{
					lua_pop(L, 1);
					lua_pushnumber(L, value);
					changed = true;
				}
				break;
			}
			case LUA_TSTRING:
			{
				size_t len = 0;
				const char* str = lua_tolstring(L, -1, &len);

				char* buf = new char[len + 64];
				memcpy(buf, str, len);
				buf[len + 63] = 0;
				buf[len] = 0;

				if (ImGui::InputText("##", buf, len + 64))
				{
					lua_pop(L, 1);
					lua_pushstring(L, buf);
					changed = true;
				}

				delete[] buf;
				break;
			}
			case LUA_TVECTOR:
			{
				const float* vec = lua_tovector(L, -1);
				float buf[3] = {};
				memcpy(buf, vec, sizeof(float) * 3);

				if (ImGui::InputFloat3("##", buf))
				{
					lua_pop(L, 1);
					lua_pushvector(L, buf[0], buf[1], buf[2]);
					changed = true;
				}
				break;
			}
			default: assert(false);
			}
		}

		ImGui::PopID();
	}
	}

	assert(!changed || CanEdit);
	return changed;
}

static bool InDebugger = false;
static ImGuiContext* debuggerContext = nullptr;
static ImGuiContext* prevContext = nullptr;
static int prevCursorMode = GLFW_CURSOR_NORMAL;

static void debugBreakHook(lua_State* L, lua_Debug* ar, ScriptEngine::L::DebugBreakReason Reason)
{
	using namespace ScriptEngine::L;
	ZoneScoped;

	if (Reason == ScriptEngine::L::DebugBreakReason::Interrupt)
	{
		lua_State* co = (lua_State*)ar->userdata;
		assert(co);

		lua_resume(co, nullptr, 0);
	}

	Engine* engine = Engine::Get();

	luaL_checkstack(L, 20, "debugger");

	std::string_view breakReason;
	std::string errorMessage;

	const std::string_view BreakReasons[] = {
		"Broke into Debugger",
		"Breakpoint hit",
		"Coroutine interrupted",
		"Error occurred",
		"Debug step"
	};

	const std::string_view BreakExplanations[] = {
		"Something caused the coroutine to enter a `BREAK` state, such as `debug.breakpoint()`",
		"The coroutine reached a breakpoint while running, such as one set by `debug.breakpoint(line)`",
		"The coroutine was interrupted by resuming another coroutine that entered a `BREAK` state",
		"Error occurred (this should have been replaced with the actual error message)",
		"You are stepping through the code"
	};

	breakReason = BreakReasons[Reason];

	if (Reason == DebugBreakReason::Error)
		errorMessage = lua_tostring(L, -1);
	else
		errorMessage = BreakExplanations[Reason];

	ScriptEngine::L::StateUserdata* corUd = (ScriptEngine::L::StateUserdata*)L->userdata;

	if (!InDebugger)
	{
		prevContext = ImGui::GetCurrentContext();
		debuggerContext = ImGui::CreateContext();

		ImGuiIO& debuggerGuiIO = ImGui::GetIO(debuggerContext);
		debuggerGuiIO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		debuggerGuiIO.IniFilename = "debugger-layout.ini";

		ImGui_ImplGlfw_Shutdown();

		ImGui::SetCurrentContext(debuggerContext);

		PHX_ENSURE_MSG(ImGui_ImplGlfw_InitForOpenGL(engine->Window, true), "Failed to initialize Dear ImGui for GLFW on Debugger init");
		PHX_ENSURE_MSG(ImGui_ImplOpenGL3_Init("#version 460"), "Failed to initialize Dear ImGui for OpenGL on Debugger init");

		prevCursorMode = glfwGetInputMode(engine->Window, GLFW_CURSOR);
		glfwSetInputMode(engine->Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

		engine->RendererContext.FrameBuffer.Unbind();
	}
	else
	{
		ImGui::SetCurrentContext(debuggerContext);
	}

	InDebugger = true;

	double debuggerLastSecond = GetRunningTime();
	double debuggerLastFrame = GetRunningTime();
	int framesPerSecond = 0;
	int framesInSecond = 0;
	bool develUI = false;
	bool quietBg = true;
	bool running = true;

	int li = 0;
	bool getInfoSuccess = lua_getinfo(L, li, "slnfu", ar);

	bool firstFrame = true;

	while (!ar->short_src || (strcmp(ar->short_src, "[C]") == 0))
	{
		if (getInfoSuccess)
			lua_pop(L, 1);
		li++;

		if (!lua_getinfo(L, li, "slnfu", ar))
		{
			getInfoSuccess = lua_getinfo(L, 0, "slnfu", ar);
			ar->short_src = "Failed to find function at call frame";
			break;
		}
		else
			getInfoSuccess = true;
	}

	int currfuncindex = getInfoSuccess ? lua_gettop(L) : 0;
	invokeTextEditor(ar->short_src ? ar->short_src : "!InlineDocument:Unknown source").JumpToLine = ar->currentline;

	static bool s_CallstackJumpToCurrentThread = false;
	s_CallstackJumpToCurrentThread = true;

	static int CurrentVMIndex = 0;

	for (auto it = ScriptEngine::VMs.begin(); it != ScriptEngine::VMs.end(); it++)
	{
		if (it->first == corUd->VM)
			break;
		CurrentVMIndex++;
	}

	while (!glfwWindowShouldClose(engine->Window) && running)
	{
		ZoneScopedN("DebuggerFrame");
		double dt = GetRunningTime() - debuggerLastFrame;
		debuggerLastFrame = GetRunningTime();

		glfwPollEvents();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGui::DockSpaceOverViewport();

		glClear(GL_COLOR_BUFFER_BIT);

		if (ImGui::Begin("Debugger"))
		{
			ImGui::TextUnformatted(breakReason.data());

			ImGui::Text("Script: %s", ar->short_src);
			ImGui::Text("Line: %i", ar->currentline);
			ImGui::Text("In: %s", ar->name ? ar->name : "<anonymous function>");
			ImGui::TextUnformatted(errorMessage.c_str());
			ImGui::SetItemTooltip("%s", errorMessage.c_str());
			ImGui::Text("VM: %s", corUd->VM.c_str());

			if (ImGui::Button("Resume (F5)") || ImGui::IsKeyDown(ImGuiKey_F5))
			{
				lua_callbacks(L)->debugstep = nullptr;
				running = false;
			}

			static bool s_WasF7Down = false;
			static bool s_WasF8Down = false;
			bool isF7Down = ImGui::IsKeyDown(ImGuiKey_F7) && running;
			bool isF8Down = ImGui::IsKeyDown(ImGuiKey_F8) && running;

			if (isF8Down)
			{
				if (s_WasF8Down)
					isF8Down = false;

				s_WasF8Down = true;
			}
			else
				s_WasF8Down = false;

			if (ScriptEngine::L::DebugBreak && Reason != DebugBreakReason::Error && (ImGui::Button("Step over (F7)") || (isF7Down && !s_WasF7Down)))
			{
				ImGui::SaveIniSettingsToDisk("debugger-layout.ini");

				if (Reason == ScriptEngine::L::DebugBreakReason::DebuggerStep)
				{
					running = false;
					ImGui::End();
					ImGui::EndFrame();

					assert((lua_gettop(L) == currfuncindex && lua_type(L, -1) == LUA_TFUNCTION) || currfuncindex == 0);
					if (currfuncindex != 0)
						lua_pop(L, 1); // pop our function from `lua_getinfo`

					ImGui::SetCurrentContext(prevContext);

					s_WasF7Down = true;
					return;
				}
				else if (Reason == ScriptEngine::L::DebugBreakReason::Breakpoint)
				{
					lua_breakpoint(L, -1, ar->currentline, false); // please
				}

				static int s_PrevLine = 0;
				s_PrevLine = ar->currentline;

				lua_callbacks(L)->debugstep = [](lua_State* L, lua_Debug* ar)
					{
						if (ar->currentline > s_PrevLine)
						{
							s_PrevLine = ar->currentline;
							debugBreakHook(L, ar, ScriptEngine::L::DebugBreakReason::DebuggerStep);
						}
					};

				lua_singlestep(L, true);
				lua_break(L);
				running = false;
			}

			s_WasF7Down = isF7Down;

			if (ScriptEngine::L::DebugBreak && Reason != DebugBreakReason::Error && (ImGui::Button("Single-step (F8)") || isF8Down))
			{
				ImGui::SaveIniSettingsToDisk("debugger-layout.ini");

				if (Reason == ScriptEngine::L::DebugBreakReason::DebuggerStep)
				{
					running = false;
					ImGui::End();
					ImGui::EndFrame();

					assert((lua_gettop(L) == currfuncindex && lua_type(L, -1) == LUA_TFUNCTION) || currfuncindex == 0);
					if (currfuncindex != 0)
						lua_pop(L, 1); // pop our function from `lua_getinfo`

					ImGui::SetCurrentContext(prevContext);

					return;
				}
				else if (Reason == ScriptEngine::L::DebugBreakReason::Breakpoint)
				{
					lua_breakpoint(L, -1, ar->currentline, false); // please
				}

				static int s_PrevLine = 0;
				s_PrevLine = ar->currentline;

				lua_callbacks(L)->debugstep = [](lua_State* L, lua_Debug* ar)
					{
						if (ar->currentline != s_PrevLine)
						{
							s_PrevLine = ar->currentline;
							debugBreakHook(L, ar, ScriptEngine::L::DebugBreakReason::DebuggerStep);
						}
					};

				lua_singlestep(L, true);
				lua_break(L);
				running = false;
			}

			ImGui::Checkbox("All Developer UIs", &develUI);
			ImGui::Checkbox("Quiet Background", &quietBg);
			ImGui::Text("Debugger %i FPS / %.2fms", framesPerSecond, dt);

			if (ScriptEngine::L::DebugBreak)
			{
				if (ImGui::Button("Detach Debugger for this VM and all VMs created in the future"))
				{
					lua_Callbacks* cb = lua_callbacks(L);
					ScriptEngine::L::DebugBreak = nullptr;

					cb->debugbreak = nullptr;
					cb->debugprotectederror = nullptr;
					cb->debugstep = nullptr;
					cb->debuginterrupt = [](lua_State*, lua_Debug* ar)
						{
							lua_resume((lua_State*)ar->userdata, nullptr, 0);
						};
				}
			}
			else
			{
				ImGui::Text("The Debugger has been detached for VM %s and all future VMs.", corUd->VM.c_str());
			}
		}
		ImGui::End();

		invokeTextEditor(ar->short_src ? ar->short_src : "!InlineDocument:Unknown source").DebuggerCurrentLine = ar->currentline;

		if (ImGui::Begin("Watch"))
		{
			static int Section = 0;
			ImGui::Combo("Variables", &Section, "Locals\0Upvalues\0Environment\0Registry\0Stack\0");

			ImGui::BeginChild("VariablesSection", ImVec2(), ImGuiChildFlags_Borders);
			int initialStatus = lua_status(L);
			L->status = LUA_OK; // avoid hitting assertion due to potential calls to `__tostring` metamethods
			
			switch (Section)
			{
			case 0:
			{
				for (int l = 0; l < lua_stackdepth(L); l++)
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

						if (debugVariable(L) && lua_setlocal(L, l, i))
							lua_pop(L, 1);
						else
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

					if (debugVariable(L) && lua_setupvalue(L, currfuncindex, i))
						lua_pop(L, 1);
					else
						lua_pop(L, 2);
				
					lua_pop(L, 1);
				}

				break;
			}
			case 2:
			{
				lua_pushstring(L, "Environment");
				lua_pushvalue(L, LUA_ENVIRONINDEX);
				debugVariable(L);

				lua_pop(L, 2);
				break;
			}
			case 3:
			{
				lua_pushstring(L, "Registry");
				lua_pushvalue(L, LUA_REGISTRYINDEX);
				debugVariable(L);

				lua_pop(L, 2);
				break;
			}
			case 4:
			{
				for (int i = 1; i <= lua_gettop(L); i++)
				{
					lua_pushinteger(L, i);
					lua_pushvalue(L, i);
					if (debugVariable(L))
					{
						lua_remove(L, -2);
						lua_remove(L, -2);
					}

					lua_pop(L, 2);
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
			std::vector<std::string_view> vmNames;
			vmNames.reserve(ScriptEngine::VMs.size());

			for (auto& [ name, _ ] : ScriptEngine::VMs)
				vmNames.emplace_back(name);

			if (CurrentVMIndex >= (int)vmNames.size())
				CurrentVMIndex = 0;

			ImGui::Combo("##", &CurrentVMIndex, [](void* vmsPtr, int index) -> const char*
				{
					const auto vms = (std::vector<std::string_view>*)vmsPtr;
					return vms->at(index).data();
				}, &vmNames, (int)vmNames.size());

			const ScriptEngine::LuauVM& vm = ScriptEngine::VMs[std::string(vmNames[CurrentVMIndex])];

			size_t numCoroutineIdChars = size_t((ImGui::GetContentRegionAvail().x * 1.2f) / ImGui::CalcTextSize("").y);
			if (numCoroutineIdChars < 3)
				numCoroutineIdChars = 3;

			const auto renderCoroutine = [&L, ar, vm, vmNames, &currfuncindex, numCoroutineIdChars](lua_State* coroutine)
			{
				ImGui::PushID(coroutine);

				lua_Debug car = {};

				int li = 0;
				bool getInfoSuccess = lua_getinfo(coroutine, li, "slnfu", &car);

				while (!car.short_src || (strcmp(car.short_src, "[C]") == 0))
				{
					if (getInfoSuccess)
						lua_pop(coroutine, 1);
					li++;

					if (!lua_getinfo(coroutine, li, "slnfu", &car))
					{
						getInfoSuccess = lua_getinfo(coroutine, 0, "slnfu", &car);
						car.short_src = nullptr;
						break;
					}
					else
						getInfoSuccess = true;
				}

				const auto ud = (ScriptEngine::L::StateUserdata*)coroutine->userdata;
				std::string identifier = std::format("{}:{}", car.short_src ? car.short_src : (ud ? ud->SpawnTrace : "MainThread"), car.currentline);
				std::string targetFile = car.short_src ? car.short_src : "";
				int targetLine = ar->currentline;

				if (targetFile.size() == 0)
				{
					if (size_t pathBegin = identifier.find('.'); pathBegin != std::string::npos)
					{
						std::string_view fromPathOnwards = { identifier.begin() + pathBegin, identifier.end() };

						if (size_t pathEnd = fromPathOnwards.find(".luau"); pathEnd != std::string::npos)
						{
							targetFile = std::string(identifier.begin() + pathBegin, identifier.begin() + pathBegin + pathEnd + strlen(".luau"));

							if (size_t lineBegin = fromPathOnwards.find(':'); lineBegin != std::string::npos)
							{
								std::string_view fromLineOnwards = { fromPathOnwards.begin() + lineBegin, fromPathOnwards.end() };

								if (size_t lineEnd = fromLineOnwards.find('\n'); lineEnd != std::string::npos)
									targetLine = std::stoi(std::string(fromLineOnwards.begin() + 1, fromLineOnwards.begin() + lineEnd));
							}
						}
					}
				}

				bool noSourceInformation = false;

				if (identifier.size() == 2) // ":0"
				{
					identifier = "<coroutine>";
					noSourceInformation = true;
				}

				if (size_t pathStartLoc = identifier.find("scripts/"); pathStartLoc != std::string::npos)
					identifier = { identifier.begin() + pathStartLoc, identifier.end() };

				if (size_t newlLoc = identifier.find('\n'); newlLoc != std::string::npos && newlLoc != 0)
					identifier = { identifier.begin(), identifier.begin() + newlLoc };

				if (!car.short_src && ud) // means we are showing the spawn trace and not the actual current call frame
					identifier = "Spawned from " + identifier;

				if (coroutine == L)
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
				else if (coroutine == vm.MainThread || noSourceInformation)
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5, 0.5, 0.5, 1.f));
				else
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.f));

				std::string treeNodeIdentifier = identifier;
				if (treeNodeIdentifier.size() > numCoroutineIdChars)
					treeNodeIdentifier = std::string(treeNodeIdentifier.begin(), treeNodeIdentifier.begin() + numCoroutineIdChars - 3) + "...";

				bool open = ImGui::TreeNodeEx(coroutine, ImGuiTreeNodeFlags_DrawLinesFull | ImGuiTreeNodeFlags_OpenOnArrow, "%s", treeNodeIdentifier.c_str());
				ImGui::PopStyleColor();
				ImGui::SetItemTooltip("%s", identifier.c_str());

				ImGui::SameLine();

				ImGui::SetCursorPosX(ImGui::GetWindowSize().x * 0.75f + ImGui::CalcTextSize("OK FIN").x);

				const LuauStatusDisplayInfo& lsdi = LuauStatuses[lua_status(coroutine)];
				const LuauCoroutineStatusDisplayInfo& lcsdi = LuauCoroutineStatuses[lua_costatus(nullptr, coroutine)];

				ImGui::PushStyleColor(ImGuiCol_Text, lsdi.Color);
				ImGui::TextUnformatted(lsdi.Id);
				ImGui::PopStyleColor();
				ImGui::SetItemTooltip("%s", lsdi.Description);

				ImGui::SameLine();

				ImGui::PushStyleColor(ImGuiCol_Text, lcsdi.Color);
				ImGui::TextUnformatted(lcsdi.Id);
				ImGui::PopStyleColor();
				ImGui::SetItemTooltip("%s", lcsdi.Description);

				if (s_CallstackJumpToCurrentThread && coroutine == L)
				{
					ImGui::SetScrollHereY();
					s_CallstackJumpToCurrentThread = false;
				}

				if (ImGui::IsItemClicked())
				{
					assert(currfuncindex == 0 || lua_type(L, -1) == LUA_TFUNCTION);
					if (currfuncindex != 0)
						lua_remove(L, currfuncindex);

					L = coroutine;
					currfuncindex = getInfoSuccess ? lua_gettop(L) : 0;
					assert(currfuncindex == 0 || lua_type(L, currfuncindex) == LUA_TFUNCTION);
					memcpy(ar, &car, sizeof(lua_Debug));

					TextEditorTab* tab = nullptr;

					if (ud)
					{
						tab = &invokeTextEditor(identifier != "<coroutine>" ? targetFile : std::format(
							"!InlineDocument:Coroutine is not inside a function\n\nVM: {}\nSpawn trace:\n{}",
							ud->VM, ud->SpawnTrace
						));
					}
					else
					{
						tab = &invokeTextEditor(std::format("!InlineDocument:Coroutine is the main thread for VM {}", vmNames[CurrentVMIndex]));
					}

					tab->DebuggerCurrentLine = targetLine;
					tab->JumpToLine = targetLine;
				}
				else
				{
					if (getInfoSuccess)
						lua_pop(coroutine, 1); // pop our current function off to leave the stack balanced
				}

				if (open)
				{
					lua_Debug car = {};
					int i = 0;

					if (!lua_getinfo(coroutine, 0, "sln", &car))
						i = 1;

					for (i = 0; lua_getinfo(coroutine, i, "sln", &car); i++)
					{
						ImGui::PushID(i);

						if (car.currentline > 0)
						{
							ImGui::PushStyleColor(
								ImGuiCol_TextLink,
								ImVec4(1.f, 1.f, 1.f, 1.f) - ImGui::GetStyleColorVec4(ImGuiCol_WindowBg) + ImVec4(0.f, 0.f, 0.f, 1.f)
							);

							if (ImGui::TextLink(std::format(
								"{}:{} in {}",
								car.short_src, car.currentline, car.name ? car.name : "<anonmyous>"
							).c_str()))
							{
								invokeTextEditor(car.short_src).JumpToLine = car.currentline;
								lua_getinfo(coroutine, i, "sln", ar);
							}

							ImGui::SetItemTooltip("View source");
							ImGui::PopStyleColor();
						}
						else
						{
							ImGui::Text("%s in %s", car.short_src, car.name);
							ImGui::SetItemTooltip("Cannot view the source of functions defined in C++");
						}

						ImGui::PopID();
					}

					if (coroutine->userdata)
					{
						const ScriptEngine::L::StateUserdata* ud = (const ScriptEngine::L::StateUserdata*)coroutine->userdata;
						ImGui::SeparatorText("Spawn trace");
						ImGui::TextUnformatted(ud->SpawnTrace.c_str());
					}

					ImGui::TreePop();
				}

				ImGui::PopID();
			};

			renderCoroutine(vm.MainThread);
			for (lua_State* coroutine : ((ScriptEngine::L::StateUserdata*)vm.MainThread->userdata)->Coroutines)
				renderCoroutine(coroutine);
		}
		ImGui::End();

		if (develUI)
			engine->OnFrameRenderGui.Fire(dt);
		else
			renderTextEditors();

		if (!quietBg)
		{
			EcCamera* sceneCam = engine->WorkspaceRef->FindComponent<EcWorkspace>()->GetSceneCamera()->FindComponent<EcCamera>();

			engine->RendererContext.DrawScene(
				engine->CurrentScene,
				sceneCam->GetRenderMatrix((float)engine->WindowSizeX / engine->WindowSizeY),
				sceneCam->GetWorldTransform(),
				GetRunningTime(),
				engine->DebugWireframeRendering
			);
		}

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// there's this really obnoxious single-frame flash that occurs when single stepping :(
		if (!firstFrame)
			engine->RendererContext.SwapBuffers();
		else
			firstFrame = false;

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

	assert((lua_gettop(L) == currfuncindex && lua_type(L, -1) == LUA_TFUNCTION) || currfuncindex == 0);
	if (currfuncindex != 0)
		lua_pop(L, 1); // pop our function from `lua_getinfo`

	ImGui::SetCurrentContext(prevContext);

	if (glfwWindowShouldClose(engine->Window))
		ScriptEngine::L::DebugBreak = nullptr;
}

static void debuggerLeaveCallback()
{
	if (!InDebugger)
		return;

	ImGui::SetCurrentContext(debuggerContext);

	Engine* engine = Engine::Get();

	for (TextEditorTab& tab : s_TextEditors)
		tab.DebuggerCurrentLine = 0;
	glfwSetInputMode(engine->Window, GLFW_CURSOR, prevCursorMode);

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::SetCurrentContext(prevContext);
	ImGui::DestroyContext(debuggerContext);

	debuggerContext = nullptr;
	prevContext = nullptr;

	PHX_ENSURE_MSG(ImGui_ImplGlfw_InitForOpenGL(engine->Window, true), "Failed to initialize Dear ImGui for GLFW on Debugger shutdown");

	InDebugger = false;
}
