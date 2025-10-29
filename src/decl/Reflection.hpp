// API Reflection

#pragma once

#include <unordered_map>
#include <functional>
#include <string>
#include <vector>
#include <format>
#include <span>
#include <glm/mat4x4.hpp>

#define REFLECTION_INHERITAPI(base) {                                      \
const Reflection::PropertyMap& props = Object_##base::s_GetProperties();   \
const Reflection::MethodMap& funcs = Object_##base::s_GetMethodss();       \
s_Api.Properties.insert(                                                   \
	props.begin(),                                                         \
	props.end()                                                            \
);                                                                         \
s_Api.Methods.insert(                                                      \
		funcs.begin(),                                                     \
		funcs.end()                                                        \
);                                                                         \
auto& lin = Object_##base::s_GetLineage();                                 \
std::copy(lin.begin(), lin.end(), std::back_inserter(s_Api.Lineage));      \
s_Api.Lineage.push_back(#base);                                            \
}                                                                          \

// Declare a property with a custom Getter and Setter
#define REFLECTION_DECLAREPROP(strname, type, get, set) s_Api.Properties[strname] = Reflection::PropertyDescriptor  \
	{	                                                                                                  \
		Reflection::ValueType::type,                                                                      \
		get,                                                                                              \
		set                                                                                               \
	}                                                                                                     \

// Declare a property with the preset Getter (return x) and Setter (x = y)
#define REFLECTION_DECLAREPROP_SIMPLE(c, name, type) REFLECTION_DECLAREPROP(   \
	#name,                                                                     \
	type,                                                                      \
	[](void* p)                                                                \
	{                                                                          \
		return (Reflection::GenericValue)static_cast<c*>(p)->name;             \
	},                                                                         \
	[](void* p, const Reflection::GenericValue& gv)                            \
	{                                                                          \
		static_cast<c*>(p)->name = gv.As##type();                              \
	}                                                                          \
)                                                                              \

// Declare a STRING property with the preset Getter (return x) and Setter (x = y)
#define REFLECTION_DECLAREPROP_SIMPSTR(c, name)      REFLECTION_DECLAREPROP(   \
	#name,                                                                     \
	String,                                                                    \
	[](void* p)                                                                \
	{                                                                          \
		return (Reflection::GenericValue)static_cast<c*>(p)->name;             \
	},                                                                         \
	[](void* p, const Reflection::GenericValue& gv)                            \
	{                                                                          \
		static_cast<c*>(p)->name = gv.AsStringView();                          \
	}                                                                          \
)                                                                              \

// Declare a property with the preset Getter (return x) and Setter (x = y),
// statically-casting `y` into `cast`. Useful for getting rid of "possible loss of data"
// warnings when converting between similar types (like `double` and `float`)
#define REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(c, name, type, cast) REFLECTION_DECLAREPROP(    \
	#name,                                                                                       \
	type,                                                                                        \
	[](void* p)                                                                                  \
	{                                                                                            \
		return (Reflection::GenericValue)static_cast<c*>(p)->name;                               \
	},                                                                                           \
	[](void* p, const Reflection::GenericValue& gv)                                              \
	{                                                                                            \
		static_cast<c*>(p)->name = static_cast<cast>(gv.As##type());                             \
	}                                                                                            \
)  

// Same as above, but for Complex Types that use Pointer (Vector3 and Color)
// Calls their `operator Reflection::GenericValue`
#define REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(c, name, type) REFLECTION_DECLAREPROP(    \
	#name,                                                                               \
	type,                                                                                \
	[](void* p)                                                                          \
	{                                                                                    \
		return static_cast<c*>(p)->name.ToGenericValue();                                \
	},                                                                                   \
	[](void* p, const Reflection::GenericValue& gv)                                      \
	{                                                                                    \
		static_cast<c*>(p)->name = type(gv);                                             \
	}                                                                                    \
)                                                                                        \

// Declare a property with the preset Getter, but no Setter
#define REFLECTION_DECLAREPROP_SIMPLE_READONLY(c, name, type) REFLECTION_DECLAREPROP(    \
	#name,                                                                               \
	type,                                                                                \
	[](void* p)                                                                          \
	{                                                                                    \
		return Reflection::GenericValue(static_cast<c*>(p)->name);                       \
	},                                                                                   \
	nullptr                                                                              \
)                                                                                        \

#define REFLECTION_DECLAREFUNC(strname, ...) s_Api.Methods[strname] = Reflection::MethodDescriptor    \
	{                                                                                       \
		__VA_ARGS__                                                                         \
	}                                                                                       \

#define REFLECTION_EVENT(c, n, ...) { \
	#n, \
	{ \
		{ __VA_ARGS__ }, \
		[](void* p, Reflection::EventCallback Callback) \
		-> uint32_t \
        { \
            c* ec = static_cast<c*>(p); \
            ec->n##Callbacks.push_back(Callback); \
            return (uint32_t)ec->n##Callbacks.size() - 1; \
        }, \
        [](void* p, uint32_t Id) \
		-> void \
        { \
            static_cast<c*>(p)->n##Callbacks[Id] = nullptr; \
        } \
	} \
} \

// 01/09/2024:
// MUST be added to the `public` section of *all* objects so
// any APIs they declare can be found
// 29/11/2024: moved into `Reflection.hpp` from `GameObject.hpp`
#define REFLECTION_DECLAREAPI static const Reflection::PropertyMap& s_GetProperties()  \
{                                                                                      \
	return s_Api.Properties;                                                           \
}                                                                                      \
                                                                                       \
static const Reflection::MethodMap& s_GetMethods()                                     \
{                                                                                      \
	return s_Api.Methods;                                                              \
}                                                                                      \

#define REFLECTION_SIGNAL(CbList, ...) { ZoneScopedN(#CbList); \
	for (size_t i = 0; i < CbList.size(); i++) \
		if (const Reflection::EventCallback& cb = CbList[i]; cb) \
			cb({ __VA_ARGS__ }); \
} \

#define REFLECTION_OPTIONAL(t) (Reflection::ValueType)((uint8_t)t + (uint8_t)Reflection::ValueType::Null)

namespace Reflection
{
	enum class ValueType : uint8_t
	{
		Boolean = 0,
		Integer,
		Double,
		String,

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

		// BEFORE Null
		__lastBase,

		Null = 0b10000000
	};
	static_assert((uint8_t)ValueType::__lastBase < (uint8_t)ValueType::Null);

	std::string TypeAsString(ValueType);

	struct GenericValue;
	typedef std::function<std::vector<Reflection::GenericValue>(const std::vector<Reflection::GenericValue>&)> GenericFunction;

	struct GenericValue
	{
		Reflection::ValueType Type = Reflection::ValueType::Null;
		union ValueData
		{
			bool Bool;
			int64_t Int;
			double Double;
			char* Str;
			char StrSso[12]; // small string optimization. largest elem is glm::vec3 of 12 bytes
			glm::vec2 Vec2;
			glm::vec3 Vec3;
			GenericFunction* Func;
			void* Ptr;
		} Val = {};
		// Array length or allocated memory for `Value`
		size_t Size = 0;

		GenericValue();
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
		GenericValue(const GenericFunction&);
		GenericValue(const std::span<GenericValue>&);
		GenericValue(const std::vector<GenericValue>&);
		GenericValue(const std::unordered_map<GenericValue, GenericValue>&);

		GenericValue(const GenericValue&);
		GenericValue(GenericValue&&);

		static void CopyInto(GenericValue&, const GenericValue&);

		void Reset();

		std::string ToString() const;
		
		// Throws errors if the type does not match
		std::string_view AsStringView() const;
		bool AsBoolean() const;
		double AsDouble() const;
		int64_t AsInteger() const;
		const glm::vec2 AsVector2() const;
		glm::vec3& AsVector3() const;
		glm::mat4& AsMatrix() const;
		GenericFunction& AsFunction() const;
		std::span<GenericValue> AsArray() const;
		std::unordered_map<GenericValue, GenericValue> AsMap() const;

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

	typedef std::function<void(const std::vector<Reflection::GenericValue>&)> EventCallback;
	typedef uint32_t(*EventConnectFunction)(void*, EventCallback);
	typedef void(*EventDisconnectFunction)(void*, uint32_t);

	struct PropertyDescriptor
	{
		Reflection::ValueType Type{};

		PropertyGetter Get;
		PropertySetter Set;
		
		bool Serializes = true;
	};

	struct MethodDescriptor
	{
		std::vector<ValueType> Inputs;
		std::vector<ValueType> Outputs;

		MethodFunction Func;
	};

	struct EventDescriptor
	{
		std::vector<ValueType> CallbackInputs;

		EventConnectFunction Connect;
		EventDisconnectFunction Disconnect;
	};

	typedef std::unordered_map<std::string_view, Reflection::PropertyDescriptor> StaticPropertyMap;
	typedef std::unordered_map<std::string_view, Reflection::MethodDescriptor> StaticMethodMap;
	typedef std::unordered_map<std::string_view, Reflection::EventDescriptor> StaticEventMap;

	typedef std::unordered_map<std::string_view, const Reflection::PropertyDescriptor*> PropertyMap;
	typedef std::unordered_map<std::string_view, const Reflection::MethodDescriptor*> MethodMap;
	typedef std::unordered_map<std::string_view, const Reflection::EventDescriptor*> EventMap;

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
