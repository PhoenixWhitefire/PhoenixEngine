// Engine service GameObject component

#include <tinyfiledialogs.h>
#include <tracy/Tracy.hpp>
#include <lualib.h>

#include "component/EngineService.hpp"
#include "datatype/JsonGenerics.hpp"
#include "asset/TextureManager.hpp"
#include "GlobalJsonConfig.hpp"
#include "Version.hpp"
#include "Engine.hpp"
#include "FileRW.hpp"

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
                return { JsonToGeneric(EngineJsonConfig[inputs[0].AsStringView()]) };
            }
        } },

        { "SetConfigValue", Reflection::MethodDescriptor{
            { Reflection::ValueType::String, Reflection::ValueType::Any },
            {},
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::vector<Reflection::GenericValue>
            {
                EngineJsonConfig[inputs[0].AsStringView()] = GenericToJson(inputs[1]);

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
