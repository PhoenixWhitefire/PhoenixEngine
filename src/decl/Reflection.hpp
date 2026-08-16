// API Reflection

#pragma once

#include <initializer_list>
#include <unordered_map>
#include <functional>
#include <string>
#include <vector>
#include <future>
#include <span>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tracy/Tracy.hpp>

#include "datatype/EntityComponent.hpp"
#include "datatype/Ref.hpp"
#include "script/InputEvent.hpp"

#define REFLECTION_PROPERTY_GET_SIMPLE(c, n) [](void* p)->Reflection::GenericValue { return static_cast<c*>(p)->n; }
#define REFLECTION_PROPERTY_GET_SIMPLE_TGN(c, n) [](void* p)->Reflection::GenericValue { return static_cast<c*>(p)->n.ToGenericValue(); }
#define REFLECTION_PROPERTY_SET_SIMPLE(c, n, t) [](void* p, const Reflection::GenericValue& gv)->void { static_cast<c*>(p)->n = gv.As##t(); }
#define REFLECTION_PROPERTY_SET_SIMPLE_CTOR(c, n, t) [](void* p, const Reflection::GenericValue& gv)->void { static_cast<c*>(p)->n = t(gv); }

#define REFLECTION_PROPERTY(strn, t, g, s) { strn, { .Name = strn, .Get = (Reflection::PropertyGetter)g, .Set = (Reflection::PropertySetter)s, .Type = Reflection::ValueType::t } }

#define REFLECTION_PROPERTY_SIMPLE(c, n, t) REFLECTION_PROPERTY(#n, t, REFLECTION_PROPERTY_GET_SIMPLE(c, n), REFLECTION_PROPERTY_SET_SIMPLE(c, n, t))
#define REFLECTION_PROPERTY_SIMPLE_NGV(c, n, t) REFLECTION_PROPERTY(#n, t, REFLECTION_PROPERTY_GET_SIMPLE_TGN(c, n), REFLECTION_PROPERTY_SET_SIMPLE_CTOR(c, n, t))

#define REFLECTION_EVENT(c, n, ...) { \
    #n, \
    Reflection::EventDescriptor{ \
        .CallbackInputs = { __VA_OPT__(REFLECTION_SPAN({ __VA_ARGS__ })) }, \
        .Connect = [](void* p, const Reflection::EventConnection& Callback) \
        -> uint32_t \
        { \
            c* ec = static_cast<c*>(p); \
            return Reflection::EventConnect(ec->n##Callbacks, Callback); \
        }, \
        .Disconnect = [](void* p, uint32_t Id) \
        -> void \
        { \
            c* ec = static_cast<c*>(p); \
            Reflection::EventDisconnect(ec->n##Callbacks, Id); \
        }, \
        .Cleanup = [](void* p) \
        -> void \
        { \
            c* ec = static_cast<c*>(p); \
            Reflection::EventCleanup(ec->n##Callbacks); \
        } \
    } \
}

#define REFLECTION_EVENT_DEBUGGERDISCARD(c, n, ...) { \
    #n, \
    Reflection::EventDescriptor{ \
        .CallbackInputs = { __VA_OPT__(REFLECTION_SPAN({ __VA_ARGS__ })) }, \
        .Connect = [](void* p, const Reflection::EventConnection& Callback) \
        -> uint32_t \
        { \
            c* ec = static_cast<c*>(p); \
            return Reflection::EventConnect(ec->n##Callbacks, Callback); \
        }, \
        .Disconnect = [](void* p, uint32_t Id) \
        -> void \
        { \
            c* ec = static_cast<c*>(p); \
            Reflection::EventDisconnect(ec->n##Callbacks, Id); \
        }, \
        .Cleanup = [](void* p) \
        -> void \
        { \
            c* ec = static_cast<c*>(p); \
            Reflection::EventCleanup(ec->n##Callbacks); \
        }, \
        .DebuggerShouldDiscard = true, \
    } \
}

#define REFLECTION_OPTIONAL(ty) (Reflection::ValueType)(Reflection::ValueType::ty + Reflection::ValueType::Null)

// bleh
#define REFLECTION_SPAN(...) ([]() -> std::span<const Reflection::ValueType> { \
    static constexpr Reflection::ValueType array[] = __VA_ARGS__; \
    return array; \
}())

#define REFLECTION_GV_SSO sizeof(glm::mat4)

class GameObject;

namespace Reflection
{
    struct ValueType_
    {
    enum VT : uint8_t {
        Boolean = 1, // first index MUST be 1!! for Null checking!!!
        Integer,
        Double,
        String,
        Buffer,

        Color,
        Vector2,
        Vector3,
        Matrix,

        GameObject,
        Function,

        // 12/08/2024:
        // Yep, it's all coming together now...
        // Why have a GenericValueArray, when a GenericValue can simply BE an Array?
        Array,
        // Keys will be Strings. Odd items of GenericValue.Array will be the keys, even items will be the values
        Map,

        EventSignal,
        InputEvent,
        NumberGradient,
        VectorGradient,
        ColorGradient,

        // BEFORE Null
        lastBase,

        Null = 0b01000000,
        Any = 0b10000000 // Only for type definitions
    };
    };
    using ValueType = ValueType_::VT;

    static_assert(ValueType::lastBase < ValueType::Null);

    std::string TypeAsString(ValueType);
    // `Target` is the documented definition, `Value` is the actual value
    // e.g.: `GameObject | Null` and `GameObject`
    bool TypeFits(ValueType Target, ValueType Value);

    struct GenericValue;
    struct GenericFunction
    {
        std::function<std::vector<Reflection::GenericValue>(const std::vector<Reflection::GenericValue>&)>* Func = nullptr;
        std::function<void()>* Cleanup = nullptr;
        int Reference = INT32_MAX;
    };

    struct EventDescriptor;
    struct EventSignal
    {
        EventDescriptor* Descriptor = nullptr;
        ReflectorRef Reflector;
        const char* Name = nullptr;
        uint32_t RestrictDataModel = UINT32_MAX;
    };

    struct GenericValue
    {
        union ValueData
        {
            bool Bool;
            int64_t Int;
            double Double;
            char* Str;
            char StrSso[REFLECTION_GV_SSO]; // small string optimization. largest elem is InputAction
            glm::vec2 Vec2;
            glm::vec3 Vec3;
            GenericFunction Func;
            GenericValue* Array;
            glm::mat4 Mat;
            EventSignal Event;
            InputEvent Input;
            void* Ptr;
        } Val = {};
        // Array length or allocated memory for `Value`
        uint32_t Size = 0;
        Reflection::ValueType Type = Reflection::ValueType::Null; // at the end for better size

        GenericValue() = default;
        ~GenericValue();

        GenericValue(const std::string_view&);
        GenericValue(const std::string&);
        GenericValue(const char*);
        GenericValue(bool);
        GenericValue(double);
        GenericValue(int64_t);
        GenericValue(uint32_t);
        GenericValue(int);
        GenericValue(glm::vec2);
        GenericValue(const glm::vec3&);
        GenericValue(const glm::mat4&);
        GenericValue(const std::span<GenericValue>&);
        GenericValue(const std::vector<GenericValue>&);
        GenericValue(const std::unordered_map<GenericValue, GenericValue>&);
        GenericValue(const InputEvent&);
        GenericValue(const ObjectRef&);

        GenericValue(const GenericValue&);
        GenericValue(GenericValue&&);

        struct Pair
        {
            const GenericValue& First;
            const GenericValue& Second;
        };

        static GenericValue Null();
        static GenericValue MapPairs(const std::span<const Pair>&);
        static GenericValue MapPairs(const std::initializer_list<Pair>& initializer)
        {
            return MapPairs(std::span(initializer.begin(), initializer.end()));
        }

        static void CopyInto(GenericValue&, const GenericValue&);

        void Reset();

        std::string ToString() const;

        bool IsNull() const;

        // Throws errors if the type does not match
        std::string AsString() const;
        std::string_view AsStringView() const;
        bool AsBoolean() const;
        double AsDouble() const;
        int64_t AsInteger() const;
        glm::vec2 AsVector2() const;
        glm::vec3 AsVector3() const;
        glm::mat4 AsMatrix() const;
        const GenericFunction& AsFunction() const;
        std::span<GenericValue> AsArray() const;

        void ForEachMapPair(const std::function<void(const Reflection::GenericValue&, const Reflection::GenericValue&)>) const;

        GenericValue& operator = (const Reflection::GenericValue& Other);
        GenericValue& operator = (Reflection::GenericValue&& Other);
        bool operator == (const Reflection::GenericValue& Other) const;
        bool operator != (const Reflection::GenericValue& Other) const
        {
            return !(*this == Other);
        }
    };

    typedef Reflection::GenericValue(*PropertyGetter)(void*);
    typedef void(*PropertySetter)(void*, const Reflection::GenericValue&);
    typedef std::vector<Reflection::GenericValue>(*MethodFunction)(void*, const std::vector<Reflection::GenericValue>&);
    typedef std::promise<std::vector<Reflection::GenericValue>>*(*YieldingMethodFunction)(void*, const std::vector<Reflection::GenericValue>&);

    using EventCallbackFunction = std::function<void(const std::vector<Reflection::GenericValue>&, uint32_t, uint32_t)>;
    using EventConnectionCleanup = std::function<void()>;

    struct EventConnection
    {
        EventCallbackFunction Callback;
        EventConnectionCleanup Cleanup;

        uint32_t DataModel;
    };

    using EventConnectFunction = std::function<uint32_t(void*, const EventConnection&)>;
    using EventDisconnectFunction = std::function<void(void*, uint32_t)>;
    using EventCleanupFunction = std::function<void(void*)>;

    struct PropertyDescriptor
    {
        std::string_view Name;
        PropertyGetter Get;
        PropertySetter Set;

        Reflection::ValueType Type = ValueType::Null; // at the end for better size
        bool Serializes = true;
        bool ParallelReadSafe = true;
        bool ParallelWriteSafe = false;
    };

    struct MethodDescriptor
    {
        MethodDescriptor(
            const std::span<const ValueType>& Inputs,
            const std::span<const ValueType>& Outputs,
            const MethodFunction Func
        )
        : Parameters(Inputs),
          Returns(Outputs),
          Function(Func)
        {
        }

        MethodDescriptor(
            const std::span<const ValueType>& Inputs,
            const std::span<const ValueType>& Outputs,
            const YieldingMethodFunction YieldFunc
        )
        : Parameters(Inputs),
          Returns(Outputs),
          YieldFunction(YieldFunc),
          Yields(true)
        {
        }

        const std::span<const ValueType> Parameters;
        const std::span<const ValueType> Returns;

        YieldingMethodFunction YieldFunction = nullptr;
        MethodFunction Function = nullptr;
        bool ParallelSafe = false;
        bool Yields = false;
    };

    struct EventDescriptor
    {
        const std::span<const ValueType> CallbackInputs;

        EventConnectFunction Connect;
        EventDisconnectFunction Disconnect;
        EventCleanupFunction Cleanup;

        bool DebuggerShouldDiscard = false;
    };

    uint32_t EventConnect(std::vector<EventConnection>&, const Reflection::EventConnection&);
    void EventDisconnect(std::vector<EventConnection>&, uint32_t);
    void EventCleanup(std::vector<EventConnection>&);
    void SignalEvent(const std::vector<EventConnection>& Connections, const std::vector<GenericValue>&, const std::string_view Name);
    void SignalRestrictedEvent(uint32_t From, const std::vector<EventConnection>& Connections, const std::vector<GenericValue>&, const std::string_view Name);

    using StaticPropertyMap = std::unordered_map<std::string_view, Reflection::PropertyDescriptor>;
    using StaticMethodMap = std::unordered_map<std::string_view, Reflection::MethodDescriptor>;
    using StaticEventMap = std::unordered_map<std::string_view, Reflection::EventDescriptor>;

    using PropertyMap = std::unordered_map<std::string_view, const Reflection::PropertyDescriptor*>;
    using MethodMap = std::unordered_map<std::string_view, const Reflection::MethodDescriptor*>;
    using EventMap = std::unordered_map<std::string_view, const Reflection::EventDescriptor*>;

    struct StaticApi
    {
        StaticPropertyMap Properties;
        StaticMethodMap Methods;
        StaticEventMap Events;
    };

    struct Api
    {
        PropertyMap Properties;
        MethodMap Methods;
        EventMap Events;
    };
}

namespace std
{
    template <>
    struct hash<Reflection::GenericValue>
    {
        size_t operator()(const Reflection::GenericValue& g) const noexcept
        {
            using Reflection::ValueType;

            size_t h = std::hash<int>()(static_cast<int>(g.Type));

            switch (g.Type)
            {

            case ValueType::Boolean:
                return h ^ std::hash<bool>()(g.Val.Bool);
            case ValueType::Integer:
                return h ^ std::hash<int64_t>()(g.Val.Int);
            case ValueType::Double:
                return h ^ std::hash<double>()(g.Val.Double);
            case ValueType::String:
                return h ^ std::hash<std::string_view>()(g.AsStringView());
            case ValueType::Color:
                return h
                    ^ std::hash<float>()(g.Val.Vec3.x)
                    ^ std::hash<float>()(g.Val.Vec3.y)
                    ^ std::hash<float>()(g.Val.Vec3.z);
            case ValueType::Vector2:
                return h
                    ^ std::hash<float>()(g.Val.Vec2.x)
                    ^ std::hash<float>()(g.Val.Vec2.y);
            case ValueType::Vector3:
                return h
                    ^ std::hash<float>()(g.Val.Vec3.x)
                    ^ std::hash<float>()(g.Val.Vec3.y)
                    ^ std::hash<float>()(g.Val.Vec3.z);
            case ValueType::Matrix:
            {
                for (uint8_t i = 0; i < 4; i++)
                    for (uint8_t j = 0; j < 4; j++)
                        h ^= std::hash<float>()(g.Val.Mat[i][j]);

                return h;
            }
            case ValueType::GameObject:
                return h ^ std::hash<int64_t>()(g.Val.Int);
            case ValueType::Function:
                return h ^ std::hash<void*>()((void*)g.Val.Func.Func);
            case ValueType::Array: case ValueType::Map:
            {
                for (uint32_t i = 0; i < g.Size; i++)
                    h ^= std::hash<Reflection::GenericValue>()(g.Val.Array[i]);

                return h;
            }
            case ValueType::Null:
                return h;

            case ValueType::Buffer: case ValueType::EventSignal: case ValueType::InputEvent:
            case ValueType::NumberGradient: case ValueType::VectorGradient:
            case ValueType::ColorGradient: case ValueType::lastBase:
            case ValueType::Any:
                [[fallthrough]];
            [[unlikely]] default:
            {
                assert(false);
                return h;
            }

            }
        }
    };
}
