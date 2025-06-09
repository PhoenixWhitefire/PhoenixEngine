// API Reflection

#pragma once

#include <unordered_map>
#include <functional>
#include <string>
#include <vector>
#include <format>
#include <glm/mat4x4.hpp>

#define REFLECTION_INHERITAPI(base) {                                      \
const Reflection::PropertyMap& props = Object_##base::s_GetProperties();   \
const Reflection::FunctionMap& funcs = Object_##base::s_GetFunctions();    \
s_Api.Properties.insert(                                                   \
	props.begin(),                                                         \
	props.end()                                                            \
);                                                                         \
s_Api.Functions.insert(                                                    \
		funcs.begin(),                                                     \
		funcs.end()                                                        \
);                                                                         \
auto& lin = Object_##base::s_GetLineage();                                 \
std::copy(lin.begin(), lin.end(), std::back_inserter(s_Api.Lineage));      \
s_Api.Lineage.push_back(#base);                                            \
}                                                                          \
// The following macros (REFLECTION_DECLAREPROP, _DECLAREPROP_SIMPLE and _SIMPLE_READONLY)
// are meant to be called in member functions of Reflection::ReflectionInfo-deriving classes,
// as they use the `s_Api.Properties` member.

// Declare a property with a custom Getter and Setter
#define REFLECTION_DECLAREPROP(strname, type, get, set) s_Api.Properties[strname] = Reflection::Property \
	{	                                                                                                 \
		Reflection::ValueType::type,                                                                     \
		get,                                                                                             \
		set                                                                                              \
	}                                                                                                    \

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
#define REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(c, name, type, cast) REFLECTION_DECLAREPROP(   \
	#name,                                                                                      \
	type,                                                                                       \
	[](void* p)                                                                                 \
	{                                                                                           \
		return (Reflection::GenericValue)static_cast<c*>(p)->name;                              \
	},                                                                                          \
	[](void* p, const Reflection::GenericValue& gv)                                             \
	{                                                                                           \
		static_cast<c*>(p)->name = static_cast<cast>(gv.As##type());                            \
	}                                                                                           \
)  

// Same as above, but for Complex Types that use Pointer (Vector3 and Color)
// Calls their `operator Reflection::GenericValue`
#define REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(c, name, type) REFLECTION_DECLAREPROP(   \
	#name,                                                                              \
	type,                                                                               \
	[](void* p)                                                                         \
	{                                                                                   \
		return static_cast<c*>(p)->name.ToGenericValue();                               \
	},                                                                                  \
	[](void* p, const Reflection::GenericValue& gv)                                     \
	{                                                                                   \
		static_cast<c*>(p)->name = type(gv);                                            \
	}                                                                                   \
)                                                                                       \

// Declare a property with the preset Getter, but no Setter
#define REFLECTION_DECLAREPROP_SIMPLE_READONLY(c, name, type) REFLECTION_DECLAREPROP(   \
	#name,                                                                              \
	type,                                                                               \
	[](void* p)                                                                         \
	{                                                                                   \
		return Reflection::GenericValue(static_cast<c*>(p)->name);                      \
	},                                                                                  \
	nullptr                                                                             \
)                                                                                       \

#define REFLECTION_DECLAREFUNC(strname, ...) s_Api.Functions[strname] = Reflection::Function \
	{                                                                                        \
		__VA_ARGS__                                                                          \
	}                                                                                        \

#define REFLECTION_DECLAREPROC_INPUTLESS(name, func) {                                       \
auto name##Lambda = func;                                                                    \
REFLECTION_DECLAREFUNC(                                                                      \
	#name,                                                                                   \
	{},                                                                                      \
	{},                                                                                      \
	[name##Lambda](void* p, const Reflection::GenericValue&)                                 \
    -> std::vector<Reflection::GenericValue>                                                 \
    {                                                                                        \
		name##Lambda(p);                                                                     \
		return {};                                                                           \
	}                                                                                        \
);                                                                                           \
}                                                                                            \

// 01/09/2024:
// MUST be added to the `public` section of *all* objects so
// any APIs they declare can be found
// 29/11/2024: moved into `Reflection.hpp` from `GameObject.hpp`
#define REFLECTION_DECLAREAPI static const Reflection::PropertyMap& s_GetProperties()  \
{                                                                                      \
	return s_Api.Properties;                                                           \
}                                                                                      \
                                                                                       \
static const Reflection::FunctionMap& s_GetFunctions()                                 \
{                                                                                      \
	return s_Api.Functions;                                                            \
}                                                                                      \

namespace Reflection
{
	enum class ValueType : uint8_t
	{
		Null = 0,

		Boolean,
		Integer,
		Double,
		String,

		Color,
		Vector3,
		Matrix,

		GameObject,
		// 12/08/2024:
		// Yep, it's all coming together now...
		// Why have a GenericValueArray, when a GenericValue can simply BE an Array?
		Array,
		// Keys will be Strings. Odd items of GenericValue.Array will be the keys, even items will be the values
		Map,
		// Must ALWAYS be the last element
		__count
	};

	const std::string_view& TypeAsString(ValueType);

	struct GenericValue
	{
		Reflection::ValueType Type = Reflection::ValueType::Null;
		void* Value = nullptr;
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
		GenericValue(const glm::mat4&);
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
		glm::mat4& AsMatrix() const;
		std::vector<GenericValue> AsArray() const;
		std::unordered_map<GenericValue, GenericValue> AsMap() const;

		GenericValue& operator = (const Reflection::GenericValue& Other);
		GenericValue& operator = (Reflection::GenericValue&& Other);
		bool operator == (const Reflection::GenericValue& Other) const;
	};

	class Reflectable;

	struct Property
	{
		Reflection::ValueType Type{};

		std::function<Reflection::GenericValue(void*)> Get;
		std::function<void(void*, const Reflection::GenericValue&)> Set;
	};

	// could be a `ValueType` bitmask, but tried it and got too complicated
	// i'll just settle for this for now
	struct ParameterType
	{
		ParameterType(Reflection::ValueType t)
			: Type(t)
		{
		}

		ParameterType(Reflection::ValueType t, bool o)
			: Type(t), IsOptional(o)
		{
		}

		Reflection::ValueType Type;
		bool IsOptional = false;

		operator Reflection::ValueType () const
		{
			return Type;
		}
	};

	struct Function
	{
		std::vector<ParameterType> Inputs;
		std::vector<ParameterType> Outputs;

		std::function<std::vector<Reflection::GenericValue>(void*, const std::vector<Reflection::GenericValue>&)> Func;
	};

	typedef std::unordered_map<std::string_view, Reflection::Property> PropertyMap;
	typedef std::unordered_map<std::string_view, Reflection::Function> FunctionMap;

	struct Api
	{
		PropertyMap Properties;
		FunctionMap Functions;
	};
}
