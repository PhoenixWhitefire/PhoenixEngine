// Engine service GameObject component

#include <tinyfiledialogs.h>
#include <tracy/Tracy.hpp>
#include <lualib.h>

#include "component/EngineService.hpp"
#include "asset/TextureManager.hpp"
#include "GlobalJsonConfig.hpp"
#include "Version.hpp"
#include "Engine.hpp"
#include "FileRW.hpp"

// Also in `ScriptEngine.cpp`!!
#define JSON_ENCODED_DATA_TAG "__HX_EncodedData"

static Reflection::GenericValue jsonToGeneric(const nlohmann::json& v)
{
    switch (v.type())
	{
	case nlohmann::json::value_t::null:
        return Reflection::GenericValue::Null();

	case nlohmann::json::value_t::boolean:
        return Reflection::GenericValue((bool)v);

	case nlohmann::json::value_t::number_integer:
        return Reflection::GenericValue((int)v);

	case nlohmann::json::value_t::number_unsigned:
        return Reflection::GenericValue((uint32_t)v);

	case nlohmann::json::value_t::number_float:
        return Reflection::GenericValue((float)v);

	case nlohmann::json::value_t::string:
		return Reflection::GenericValue((std::string)v);

	case nlohmann::json::value_t::array:
	{
        std::vector<Reflection::GenericValue> arr;
		for (size_t i = 0; i < v.size(); i++)
            arr.push_back(jsonToGeneric(v[i]));

		return arr;
	}
	case nlohmann::json::value_t::object:
	{
        std::vector<Reflection::GenericValue> arr;

		for (auto it = v.begin(); it != v.end(); ++it)
		{
			std::string key = it.key();
			const nlohmann::json& data = it.value();
            arr.emplace_back(key);

            Reflection::GenericValue val;

			if (key == JSON_ENCODED_DATA_TAG)
			{
				const std::string& type = data["type"];
				const nlohmann::json& encoded = data["data"];

				if (type == "vector")
                    val = { glm::vec3(encoded[0], encoded[1], encoded[2]) };
				else
					RAISE_RT("Unknown encoded datatype '{}'", type);
			}
			else
				val = jsonToGeneric(data);

            arr.push_back(val);
		}

        Reflection::GenericValue gv = arr;
        gv.Type = Reflection::ValueType::Map;

		return gv;
	}
	default:
	{
		assert(false);
        return std::format("< JSON Value : {} >", v.type_name());
	}
	}
}

#define ERROR_CONTEXTUALIZED_NVARARGS(e) { \
if (Context.size() > 0) \
	RAISE_RT(e " (serializing {})", Context); \
else \
	RAISE_RT(e); } \

#define ERROR_CONTEXTUALIZED(e, ...) { \
if (Context.size() > 0) \
	RAISE_RT(e " in {}", __VA_ARGS__, Context); \
else \
	RAISE_RT(e, __VA_ARGS__); } \

static nlohmann::json genericToJson(const Reflection::GenericValue& Value, std::string Context = "")
{
    switch (Value.Type)
	{
	case Reflection::ValueType::Null:
		return {};

	case Reflection::ValueType::Boolean:
        return Value.AsBoolean();

	case Reflection::ValueType::Double:
        return (float)Value.AsDouble();

	case Reflection::ValueType::String:
        return Value.AsString();

    case Reflection::ValueType::Array:
    {
        nlohmann::json arr = nlohmann::json::array();
        for (uint32_t i = 0; i < Value.Size; i++)
            arr[i] = genericToJson(Value.Val.Array[i]);

        return arr;
    }

	case Reflection::ValueType::Map:
	{
		nlohmann::json t = nlohmann::json::object();
		Reflection::ValueType keytype = Reflection::ValueType::Null;

        std::span<Reflection::GenericValue> arr = Value.AsArray();

		for (uint32_t i = 0; i < arr.size(); i += 2)
		{
            const Reflection::GenericValue& k = arr[i];
            const Reflection::GenericValue& v = arr[i + 1];

			if (k.Type != keytype && keytype != Reflection::ValueType::Null)
			{
				ERROR_CONTEXTUALIZED(
					"All keys must have the same type. Previous type: {}, Current type: {} ('{}')",
					Reflection::TypeAsString(keytype), Reflection::TypeAsString(k.Type), k.ToString()
				);
			}

			if (keytype == Reflection::ValueType::Null)
			{
				keytype = k.Type;

				if (keytype != Reflection::ValueType::String)
				{
					ERROR_CONTEXTUALIZED(
						"Key type expected to be String, got '{}' ({})",
						k.ToString(), Reflection::TypeAsString(k.Type)
					);
				}
			}

			std::string key = k.AsString();

			if (key == JSON_ENCODED_DATA_TAG)
				ERROR_CONTEXTUALIZED_NVARARGS("The table key '" JSON_ENCODED_DATA_TAG "' is reserved");

			if (Context.size() == 0)
				Context = "Dictionary";

			t[key] = genericToJson(v, Context + "." + key);
		}

		return t;
	}

	case Reflection::ValueType::Vector3:
	{
		const glm::vec3& vec = Value.AsVector3();

		nlohmann::json value;
		nlohmann::json& data = value[JSON_ENCODED_DATA_TAG];
		data["type"] = "vector";
		data["data"][0] = vec[0];
		data["data"][1] = vec[1];
		data["data"][2] = vec[2];

		return value;
	}

	[[unlikely]] default:
	{
		ERROR_CONTEXTUALIZED(
			"Cannot serialize '%s' (%s) to a JSON value",
			Value.ToString(), Reflection::TypeAsString(Value.Type)
		);
	}
	}
}

const Reflection::StaticPropertyMap& EngineComponentManager::GetProperties()
{
    static const Reflection::StaticPropertyMap props = {
        REFLECTION_PROPERTY(
            "IsHeadless",
            Boolean,
            [](void*) -> Reflection::GenericValue
            {
                Engine* engine = Engine::Get();
                return engine->IsHeadlessMode;
            },
            nullptr
        ),
        REFLECTION_PROPERTY(
            "Framerate",
            Integer,
            [](void*) -> Reflection::GenericValue
            {
                Engine* engine = Engine::Get();
                return engine->FramesPerSecond;
            },
            nullptr
        ),
        REFLECTION_PROPERTY(
            "FramerateCap",
            Integer,
            [](void*) -> Reflection::GenericValue
            {
                Engine* engine = Engine::Get();
                return engine->FpsCap;
            },
            [](void*, const Reflection::GenericValue& gv)
            {
                Engine* engine = Engine::Get();
                engine->FpsCap = gv.AsInteger();
            }
        ),
        REFLECTION_PROPERTY(
            "Version",
            String,
            [](void*) -> Reflection::GenericValue
            {
                return GetEngineVersion();
            },
            nullptr
        ),
        REFLECTION_PROPERTY(
            "CommitHash",
            String,
            [](void*) -> Reflection::GenericValue
            {
                return GetEngineCommitHash();
            },
            nullptr
        ),
        REFLECTION_PROPERTY(
            "CommitTag",
            String,
            [](void*) -> Reflection::GenericValue
            {
                return GetEngineCommitTag();
            },
            nullptr
        ),
        REFLECTION_PROPERTY(
            "TargetPlatform",
            String,
            [](void*) -> Reflection::GenericValue
            {
                return PHX_TARGET_PLATFORM;
            },
            nullptr
        ),
        REFLECTION_PROPERTY(
            "BoundDataModel",
            GameObject,
            [](void*) -> Reflection::GenericValue
            {
                Engine* engine = Engine::Get();
                return engine->DataModelRef->ToGenericValue();
            },
            nullptr
        )
    };

    return props;
}

const Reflection::StaticMethodMap& EngineComponentManager::GetMethods()
{
    static const Reflection::StaticMethodMap methods = {
        { "BindDataModel", Reflection::MethodDescriptor{
            { Reflection::ValueType::GameObject },
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                Engine* engine = Engine::Get();
                engine->BindDataModel(GameObjectManager::Get()->FromGenericValue(inputs[0]));

                return {};
            }
        } },

        { "ShowMessageBox", Reflection::MethodDescriptor{
            { Reflection::ValueType::String, Reflection::ValueType::String, REFLECTION_OPTIONAL(String), REFLECTION_OPTIONAL(String), REFLECTION_OPTIONAL(Integer) },
            { Reflection::ValueType::Integer },
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                std::string title = inputs[0].AsString();
                std::string message = inputs[1].AsString();

                if (title.find('`') != std::string::npos)
                {
                    Log.WarningF("Backticks will be replaced with single-quotes in message box title '{}'", title);
                    for (char& c : title)
                    {
                        if (c == '`')
                            c = '\'';
                    }
                }
                if (message.find('`') != std::string::npos)
                {
                    Log.WarningF("Backticks will be replaced with single-quotes in message box message '{}'", message);
                    for (char& c : message)
                    {
                        if (c == '`')
                            c = '\'';
                    }
                }

                return { tinyfd_messageBox(
                    title.data(),
                    message.data(),
                    inputs.size() > 2 ? inputs[2].AsStringView().data() : "ok",   // buttons
                    inputs.size() > 3 ? inputs[3].AsStringView().data() : "info", // icon
                    inputs.size() > 4 ? inputs[4].AsInteger() : 1                 // default button
                ) };
            }
        } },

        { "GetCliArguments", Reflection::MethodDescriptor{
            {},
            { Reflection::ValueType::Array },
            [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                Engine* engine = Engine::Get();

                std::vector<Reflection::GenericValue> out;
                out.reserve(engine->argc);

                for (int i = 0; i < engine->argc; i++)
                    out.emplace_back(engine->argv[i]);

                return { Reflection::GenericValue(out) };
            }
        } },

        { "GetConfigValue", Reflection::MethodDescriptor{
            { Reflection::ValueType::String },
            { Reflection::ValueType::Any },
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                return { jsonToGeneric(EngineJsonConfig[inputs[0].AsStringView()]) };
            }
        } },

        { "SetConfigValue", Reflection::MethodDescriptor{
            { Reflection::ValueType::String, Reflection::ValueType::Any },
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                EngineJsonConfig[inputs[0].AsStringView()] = genericToJson(inputs[1]);

                return {};
            }
        } },

        { "SaveConfig", Reflection::MethodDescriptor{
            {},
            { Reflection::ValueType::Boolean, Reflection::ValueType::String },
            [](void*, const std::vector<Reflection::GenericValue>&) -> std::vector<Reflection::GenericValue>
            {
                std::string error;
                bool writeSuccess = FileRW::WriteFile("./phoenix.conf", EngineJsonConfig.dump(2), &error);

                return { writeSuccess, error };
            }
        } },
    };

    return methods;
}

void EngineComponentManager::BindService(uint32_t)
{
}

void EngineComponentManager::UnbindService()
{
}
