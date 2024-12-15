#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <ImGuiFD/ImGuiFD.h>
#include <imgui/imgui.h>
#include <glad/gl.h>
#include <fstream>

#include "Editor.hpp"
#include "gameobject/GameObjects.hpp"
#include "asset/MaterialManager.hpp"
#include "asset/TextureManager.hpp"
#include "asset/MeshProvider.hpp"
#include "Utilities.hpp"
#include "UserInput.hpp"
#include "FileRW.hpp"
#include "Debug.hpp"

constexpr uint32_t OBJECT_NEW_CLASSNAME_BUFSIZE = 16;
constexpr uint32_t MATERIAL_NEW_NAME_BUFSIZE = 32;
constexpr uint32_t MATERIAL_TEXTUREPATH_BUFSIZE = 64;
constexpr const char* MATERIAL_NEW_NAME_DEFAULT = "newmaterial";

static const char* ParentString = "[Parent]";

static bool ScriptEditorEnabled = false;
static uint32_t ScriptEditorFocus = PHX_GAMEOBJECT_NULL_ID;

static nlohmann::json DefaultNewMaterial = 
{
	{ "ColorMap", "textures/materials/plastic.png" },
	{ "specExponent", 32.f },
	{ "specMultiply", 0.5f }
};

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
	m_MtlSaveNameBuf = BufferInitialize(MATERIAL_NEW_NAME_BUFSIZE, MATERIAL_NEW_NAME_DEFAULT);
	m_MtlDiffuseBuf = BufferInitialize(MATERIAL_TEXTUREPATH_BUFSIZE);
	m_MtlSpecBuf = BufferInitialize(MATERIAL_TEXTUREPATH_BUFSIZE);
	m_MtlNormalBuf = BufferInitialize(MATERIAL_TEXTUREPATH_BUFSIZE);
	m_MtlEmissionBuf = BufferInitialize(MATERIAL_TEXTUREPATH_BUFSIZE);
	m_MtlShpBuf = BufferInitialize(MATERIAL_TEXTUREPATH_BUFSIZE);
	m_MtlNewUniformNameBuf = BufferInitialize(MATERIAL_NEW_NAME_BUFSIZE);
	m_MtlUniformNameEditBuf = BufferInitialize(MATERIAL_NEW_NAME_BUFSIZE);

	MtlEditorPreview.Initialize(256, 256);
	MtlPreviewRenderer = renderer;
	MtlPreviewCamera = (Object_Camera*)GameObject::Create("Camera");
	MtlPreviewCamera->Transform = MtlPreviewCamDefaultRotation * MtlPreviewCamOffset;
	MtlPreviewCamera->FieldOfView = 50.f;
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

static void renderScriptEditor()
{
	static char* TextEntryBuffer = nullptr;
	static size_t TextEntryBufferCapacity = 0;

	Object_Script* targetScript = dynamic_cast<Object_Script*>(GameObject::GetObjectById(ScriptEditorFocus));

	static std::fstream* ScriptFileStream = nullptr;

	if (!ScriptEditorEnabled)
	{
		if (targetScript && TextEntryBuffer)
		{
			if (ScriptFileStream)
			{
				(*ScriptFileStream) << TextEntryBuffer;
				ScriptFileStream->close();

				delete ScriptFileStream;
				ScriptFileStream = nullptr;
			}
		}

		free(TextEntryBuffer);
		TextEntryBuffer = nullptr;

		return;
	}

	if (!targetScript)
	{
		ScriptEditorEnabled = false;
		ScriptEditorFocus = PHX_GAMEOBJECT_NULL_ID;

		return;
	}

	ImGui::Begin("Script Editor", 0, ImGuiWindowFlags_NoCollapse);

	ImGui::Text("%s", targetScript->Name.c_str());

	if (ImGui::Button("Save"))
		(*ScriptFileStream) << TextEntryBuffer;

	if (ImGui::Button("Close"))
	{
		// 28/10/2024 TODO
		// hooo boy do i love intentionally dirty state
		// so i don't have to go through the effort of making ONE extra function
		ScriptEditorEnabled = false;
		ImGui::End();

		return;
	}

	if (!TextEntryBuffer)
	{
		ScriptFileStream = new std::fstream(FileRW::GetAbsolutePath(targetScript->SourceFile));
		
		std::string scriptContents = "";

		if (!(*ScriptFileStream) || !ScriptFileStream->is_open())
			scriptContents = "-- Source file '" + targetScript->SourceFile + "' could not be opened";
		else
		{
			ScriptFileStream->seekg(0, std::ios::end);

			scriptContents.resize(ScriptFileStream->tellg());
			ScriptFileStream->seekg(0, std::ios::beg);

			ScriptFileStream->read(&scriptContents[0], scriptContents.size());
		}

		TextEntryBufferCapacity = scriptContents.size() + 256;
		TextEntryBuffer = (char*)malloc(TextEntryBufferCapacity);

		CopyStringToBuffer(TextEntryBuffer, TextEntryBufferCapacity, scriptContents);
	}

	ImGui::InputTextMultiline("Script", TextEntryBuffer, TextEntryBufferCapacity);

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
			std::string bufAsStr = std::string(Buffer);
			std::string texdir = "resources/" + bufAsStr.substr(0ull, bufAsStr.find_last_of("/"));

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
		ImGui::InputText("", Buffer, MATERIAL_TEXTUREPATH_BUFSIZE);
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

// 02/09/2024
// That one Gianni Matragrano shitpost where it's a
// baguette with a doge face on a white background low-res
// with the caption "pain"
// and it's just him screaming into the mic
void Editor::m_RenderMaterialEditor()
{
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

	if (m_MtlCurItem != m_MtlPrevItem)
	{
		CopyStringToBuffer(m_MtlShpBuf, MATERIAL_TEXTUREPATH_BUFSIZE, curItem.GetShader().Name);
		CopyStringToBuffer(m_MtlSaveNameBuf, MATERIAL_TEXTUREPATH_BUFSIZE, curItem.Name);

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

	static int TypeId = 0;

	ImGui::InputText("Variable Name", m_MtlNewUniformNameBuf, MATERIAL_NEW_NAME_BUFSIZE);
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

		curItem.Uniforms[m_MtlNewUniformNameBuf] = initialValue;
	}

	std::vector<std::string> uniformsArray;
	uniformsArray.reserve(curItem.Uniforms.size());

	for (auto& it : curItem.Uniforms)
		uniformsArray.push_back(it.first);

	ImGui::ListBox(
		"",
		&SelectedUniformIdx,
		&mtlUniformIterator,
		&uniformsArray,
		static_cast<int>(curItem.Uniforms.size())
	);

	if (SelectedUniformIdx != -1 && uniformsArray.size() >= 1)
	{
		const std::string& name = uniformsArray.at(SelectedUniformIdx);
		Reflection::GenericValue& value = curItem.Uniforms.at(name);

		CopyStringToBuffer(m_MtlUniformNameEditBuf, MATERIAL_NEW_NAME_BUFSIZE, name);

		ImGui::InputText("Name", m_MtlUniformNameEditBuf, MATERIAL_NEW_NAME_BUFSIZE);

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

		std::string newName = m_MtlUniformNameEditBuf;

		if (name != newName)
			curItem.Uniforms.erase(name);

		curItem.Uniforms[newName] = newValue;
	}

	if (ImGui::Button("Load texture and shader"))
	{
		curItem.ColorMap = texManager->LoadTextureFromPath(m_MtlDiffuseBuf);

		if (curItem.MetallicRoughnessMap != 0)
			curItem.MetallicRoughnessMap = texManager->LoadTextureFromPath(m_MtlSpecBuf);

		if (curItem.NormalMap != 0)
			curItem.NormalMap = texManager->LoadTextureFromPath(m_MtlNormalBuf);

		if (curItem.EmissionMap != 0)
			curItem.EmissionMap = texManager->LoadTextureFromPath(m_MtlEmissionBuf);

		curItem.ShaderId = ShaderManager::Get()->LoadFromPath(m_MtlShpBuf);
	}

	ImGui::Checkbox("Has translucency", &curItem.HasTranslucency);
	ImGui::InputFloat("Spec pow", &curItem.SpecExponent);
	ImGui::InputFloat("Spec mul", &curItem.SpecMultiply);

	ImGui::InputText("Save As", m_MtlSaveNameBuf, MATERIAL_NEW_NAME_BUFSIZE);
	ImGui::SetItemTooltip("By default, this will be the name of the material you are editing, thus overwriting it");

	if (ImGui::Button("Save"))
	{
		nlohmann::json newMtlConfig{};

		newMtlConfig["ColorMap"] = colorMap.ImagePath;

		if (curItem.MetallicRoughnessMap != 0)
			newMtlConfig["MetallicRoughnessMap"] = metallicRoughnessMap.ImagePath;

		if (curItem.NormalMap != 0)
			newMtlConfig["NormalMap"] = normalMap.ImagePath;

		if (curItem.EmissionMap != 0)
			newMtlConfig["EmissionMap"] = emissionMap.ImagePath;

		newMtlConfig["specExponent"] = curItem.SpecExponent;
		newMtlConfig["specMultiply"] = curItem.SpecMultiply;
		newMtlConfig["HasTranslucency"] = curItem.HasTranslucency;
		newMtlConfig["Shader"] = curItem.GetShader().Name;
		
		newMtlConfig["Uniforms"] = {};

		for (auto& it : curItem.Uniforms)
		{
			Reflection::GenericValue& value = it.second;

			switch (value.Type)
			{
			case (Reflection::ValueType::Bool):
			{
				newMtlConfig["Uniforms"][it.first] = value.AsBool();
				break;
			}
			case (Reflection::ValueType::Integer):
			{
				newMtlConfig["Uniforms"][it.first] = value.AsInteger();
				break;
			}
			case (Reflection::ValueType::Double):
			{
				newMtlConfig["Uniforms"][it.first] = static_cast<float>(value.AsDouble());
				break;
			}
			}
		}

		std::string filePath = "materials/" + std::string(m_MtlSaveNameBuf) + ".mtl";

		FileRW::WriteFile(
			filePath,
			newMtlConfig.dump(2),
			true
		);
	}

	ImGui::End();
}

static uint32_t HierarchyTreeSelectionId = PHX_GAMEOBJECT_NULL_ID;
static GameObject* ObjectInsertionTarget = nullptr;

static GameObject* recursiveIterateTree(GameObject* current, bool didVisitCurSelection = false)
{
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

		else if (object != hrchSelection && !didVisitCurSelection)
		{
			std::vector<GameObject*> descs = object->GetDescendants();

			if (std::find(descs.begin(), descs.end(), hrchSelection) != descs.end())
				ImGui::SetNextItemOpen(true);
		}

		ImGui::AlignTextToFramePadding();
		bool open = ImGui::TreeNodeEx(&object->ObjectId, flags, object->Name.c_str());

		if (ImGui::IsItemClicked())
			nodeClicked = object;

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
	if (ErrorTooltipTimeRemaining > 0.f)
		ImGui::SetTooltip(ErrorTooltipMessage.c_str());

	renderScriptEditor();
	m_RenderMaterialEditor();

	if (!ImGui::Begin("Editor"))
	{
		ImGui::End();
		return;
	}

	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(227, 227, 227, 255));
	ImGui::Text("Compiled: %s", __DATE__);
	ImGui::PopStyleColor();

	ImGui::Separator();

	GameObject* selected = recursiveIterateTree(GameObject::s_DataModel);

	if (selected)
	{
		ImGui::SeparatorText(selected->ClassName.c_str());

		if (ImGui::Button("Destroy"))
		{
			selected->Destroy();
			selected = nullptr;
		}
		else
		{
			Object_Script* script = dynamic_cast<Object_Script*>(selected);

			if (script)
			{
				if (ImGui::Button("Reload"))
					script->Reload();

				if (ImGui::Button("Edit"))
				{
					ScriptEditorEnabled = true;
					ScriptEditorFocus = script->ObjectId;
				}
			}

			ImGui::SeparatorText("Properties");

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

					ImGui::Text(std::vformat("{}: {}", std::make_format_args(propName, curValStr)).c_str());

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

					free(buf);

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

					ImGui::InputFloat3("Position", pos);
					ImGui::InputFloat3("Rotation", rotdegs);

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
		}
	}

	ImGui::End();
}
