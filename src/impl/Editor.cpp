#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <imgui/imgui.h>

#include "Editor.hpp"
#include "gameobject/GameObjects.hpp"
#include "asset/MaterialManager.hpp"
#include "asset/TextureManager.hpp"
#include "Utilities.hpp"
#include "UserInput.hpp"
#include "FileRW.hpp"
#include "Debug.hpp"

constexpr uint32_t OBJECT_NEW_CLASSNAME_BUFSIZE = 16;
constexpr uint32_t MATERIAL_NEW_NAME_BUFSIZE = 32;
constexpr uint32_t MATERIAL_TEXTUREPATH_BUFSIZE = 64;
constexpr const char* MATERIAL_NEW_NAME_DEFAULT = "newmaterial";

static const char* ParentString = "[Parent]";

static std::string ErrorTexture = "textures/missing.png";

static bool ScriptEditorEnabled = false;
static uint32_t ScriptEditorFocus = PHX_GAMEOBJECT_NULL_ID;

static nlohmann::json DefaultNewMaterial{};

Editor::Editor()
{
	m_NewObjectClass = BufferInitialize(OBJECT_NEW_CLASSNAME_BUFSIZE);
	m_MtlCreateNameBuf = BufferInitialize(MATERIAL_NEW_NAME_BUFSIZE, MATERIAL_NEW_NAME_DEFAULT);
	m_MtlDiffuseBuf = BufferInitialize(MATERIAL_TEXTUREPATH_BUFSIZE);
	m_MtlSpecBuf = BufferInitialize(MATERIAL_TEXTUREPATH_BUFSIZE);
	m_MtlNormalBuf = BufferInitialize(MATERIAL_TEXTUREPATH_BUFSIZE);
	m_MtlShpBuf = BufferInitialize(MATERIAL_TEXTUREPATH_BUFSIZE);
	m_MtlNewUniformNameBuf = BufferInitialize(MATERIAL_NEW_NAME_BUFSIZE);
	m_MtlUniformNameEditBuf = BufferInitialize(MATERIAL_NEW_NAME_BUFSIZE);

	DefaultNewMaterial["albedo"] = "textures/plastic.png";
}

void Editor::Update(double DeltaTime)
{
	m_InvalidObjectErrTimeRemaining -= DeltaTime;
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

	if (!ScriptEditorEnabled)
	{
		if (targetScript && TextEntryBuffer)
			FileRW::WriteFile(targetScript->SourceFile, TextEntryBuffer, true);

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

	ImGui::Begin("Script Editor");

	ImGui::Text("%s", targetScript->Name.c_str());

	if (ImGui::Button("Save"))
	{
		FileRW::WriteFile(targetScript->SourceFile, TextEntryBuffer, true);

		free(TextEntryBuffer);
		TextEntryBuffer = nullptr;
	}

	if (ImGui::Button("Close"))
	{
		// 28/10/2024 TODO
		// hooo boy do i love intentionally dirty state
		// so i don't have to go through the effort of making ONE extra function
		ScriptEditorEnabled = false;
		return;
	}

	if (!TextEntryBuffer)
	{
		bool scriptFileExists = true;
		std::string scriptContents = FileRW::ReadFile(targetScript->SourceFile, &scriptFileExists);

		if (!scriptFileExists)
			scriptContents = "-- Source file '" + targetScript->SourceFile + "' could not be opened";

		TextEntryBufferCapacity = scriptContents.size() + 256;
		TextEntryBuffer = (char*)malloc(TextEntryBufferCapacity);

		CopyStringToBuffer(TextEntryBuffer, TextEntryBufferCapacity, scriptContents);
	}

	ImGui::InputTextMultiline("Script", TextEntryBuffer, TextEntryBufferCapacity);

	ImGui::End();
}

static void mtlEditorTexture(uint32_t TextureId)
{
	const Texture& tx = TextureManager::Get()->GetTextureResource(TextureId);

	ImGui::Image(
		tx.GpuId,
		// Scale to 256 pixels wide, while maintaining aspect ratio
		ImVec2(256.f, tx.Height * (256.f / tx.Width)),
		// Flip the Y axis. Either OpenGL or Dear ImGui is bottom-up
		ImVec2(0, 1),
		ImVec2(1, 0)
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
	ImGui::Begin("Materials");

	ImGui::InputText("New material", m_MtlCreateNameBuf, MATERIAL_NEW_NAME_BUFSIZE);

	MaterialManager* mtlManager = MaterialManager::Get();
	TextureManager* texManager = TextureManager::Get();

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
		"Active materials",
		&m_MtlCurItem,
		&mtlIterator,
		nullptr,
		static_cast<int>(loadedMaterials.size())
	);

	if (m_MtlCurItem == -1)
	{
		ImGui::End();

		return;
	}

	RenderMaterial& curItem = loadedMaterials.at(m_MtlCurItem);

	Texture& colorMap = texManager->GetTextureResource(curItem.ColorMap);
	Texture& metallicRoughnessMap = texManager->GetTextureResource(curItem.MetallicRoughnessMap);
	Texture& normalMap = texManager->GetTextureResource(curItem.NormalMap);

	static int SelectedUniformIdx = -1;

	if (m_MtlCurItem != m_MtlPrevItem)
	{
		CopyStringToBuffer(m_MtlShpBuf, MATERIAL_TEXTUREPATH_BUFSIZE, curItem.GetShader().Name);

		CopyStringToBuffer(m_MtlDiffuseBuf, MATERIAL_TEXTUREPATH_BUFSIZE, colorMap.ImagePath);
		CopyStringToBuffer(m_MtlSpecBuf, MATERIAL_TEXTUREPATH_BUFSIZE, metallicRoughnessMap.ImagePath);
		CopyStringToBuffer(m_MtlNormalBuf, MATERIAL_TEXTUREPATH_BUFSIZE, normalMap.ImagePath);

		SelectedUniformIdx = -1;
	}

	m_MtlPrevItem = m_MtlCurItem;

	ImGui::InputText("Shader", m_MtlShpBuf, MATERIAL_TEXTUREPATH_BUFSIZE);

	ImGui::InputText("Color Map", m_MtlDiffuseBuf, MATERIAL_TEXTUREPATH_BUFSIZE);
	mtlEditorTexture(curItem.ColorMap);

	bool hadSpecularTexture = curItem.MetallicRoughnessMap != 0;
	bool metallicRoughnessEnabled = hadSpecularTexture;
	ImGui::Checkbox("Has MR Map", &metallicRoughnessEnabled);

	if (metallicRoughnessEnabled)
	{
		if (!hadSpecularTexture)
			curItem.MetallicRoughnessMap = texManager->LoadTextureFromPath("textures/white.png");

		ImGui::InputText("Metallic Roughness Map", m_MtlSpecBuf, MATERIAL_TEXTUREPATH_BUFSIZE);
		mtlEditorTexture(curItem.MetallicRoughnessMap);
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

		ImGui::InputText("Normal Map", m_MtlNormalBuf, MATERIAL_TEXTUREPATH_BUFSIZE);
		mtlEditorTexture(curItem.NormalMap);
	}
	else
		curItem.NormalMap = 0;

	ImGui::Text("Uniforms");

	static int TypeId = 0;

	ImGui::InputText("Uniform Name", m_MtlNewUniformNameBuf, MATERIAL_NEW_NAME_BUFSIZE);
	ImGui::InputInt("Uniform Type", &TypeId);
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

	if (ImGui::Button("Update"))
	{
		curItem.ColorMap = texManager->LoadTextureFromPath(m_MtlDiffuseBuf);

		if (curItem.MetallicRoughnessMap != 0)
			curItem.MetallicRoughnessMap = texManager->LoadTextureFromPath(m_MtlSpecBuf);

		if (curItem.NormalMap != 0)
			curItem.NormalMap = texManager->LoadTextureFromPath(m_MtlNormalBuf);

		curItem.ShaderId = ShaderManager::Get()->LoadFromPath(m_MtlShpBuf);
	}

	ImGui::Checkbox("Has translucency", &curItem.HasTranslucency);
	ImGui::InputFloat("Spec pow", &curItem.SpecExponent);
	ImGui::InputFloat("Spec mul", &curItem.SpecMultiply);

	if (ImGui::Button("Save"))
	{
		nlohmann::json newMtlConfig{};

		newMtlConfig["ColorMap"] = colorMap.ImagePath;

		if (curItem.MetallicRoughnessMap != 0)
			newMtlConfig["MetallicRoughnessMap"] = metallicRoughnessMap.ImagePath;

		if (curItem.NormalMap != 0)
			newMtlConfig["NormalMap"] = normalMap.ImagePath;

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

		std::string filePath = "materials/" + curItem.Name + ".mtl";

		FileRW::WriteFile(
			"materials/" + curItem.Name + ".mtl",
			newMtlConfig.dump(2),
			true
		);
	}

	ImGui::End();
}

static GameObject* recursiveIterateTree(GameObject* current)
{
	static GameObject* Selected = nullptr;
	
	if (Selected)
		if (GameObject::s_WorldArray.find(Selected->ObjectId) == GameObject::s_WorldArray.end())
			Selected = nullptr;

	// https://github.com/ocornut/imgui/issues/581#issuecomment-216054349
	// 07/10/2024
	GameObject* nodeClicked = nullptr;

	for (GameObject* object : current->GetChildren())
	{
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;

		if (Selected && object == Selected)
			flags |= ImGuiTreeNodeFlags_Selected;

		if (object->GetChildren().empty())
			flags |= ImGuiTreeNodeFlags_Leaf;

		bool open = ImGui::TreeNodeEx(&object->ObjectId, flags, object->Name.c_str());

		if (ImGui::IsItemClicked())
			nodeClicked = object;
			
		if (open)
		{
			recursiveIterateTree(object);
			ImGui::TreePop();
		}
	}

	if (nodeClicked)
		if (ImGui::GetIO().KeyCtrl)
			Selected = nullptr;
		else
			Selected = nodeClicked;

	return Selected;
}

void Editor::RenderUI()
{
	renderScriptEditor();
	m_RenderMaterialEditor();

	ImGui::Begin("Editor");

	ImGui::InputText("New object", m_NewObjectClass, 32);
	bool createObject = ImGui::Button("Create");

	if (m_InvalidObjectErrTimeRemaining > 0.f)
		ImGui::Text("Invalid GameObject!");

	ImGui::TreePush("GameObject Hierarchy UI");

	GameObject* selected = recursiveIterateTree(GameObject::s_DataModel);

	ImGui::TreePop();

	if (selected)
	{
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

			ImGui::Text("Properties:");

			auto& props = selected->GetProperties();

			for (auto& propListItem : props)
			{
				const char* propName = propListItem.first.c_str();
				const IProperty& prop = propListItem.second;

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

				selected->SetPropertyValue(propName, newVal);
			}
		}
	}

	ImGui::End();

	if (createObject)
	{
		if (!GameObject::IsValidObjectClass(m_NewObjectClass))
			m_InvalidObjectErrTimeRemaining = 2.f;
		else
		{
			GameObject* newObj = GameObject::Create(m_NewObjectClass);

			newObj->SetParent(selected ? selected : GameObject::s_DataModel);
		}
	}
}
