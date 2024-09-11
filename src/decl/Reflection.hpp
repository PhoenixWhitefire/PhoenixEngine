// API Reflection

#pragma once

#include<unordered_map>
#include<functional>
#include<string>
#include<vector>
#include<format>
#include<glm/mat4x4.hpp>

#define REFLECTION_INHERITAPI(base) {                 \
const PropertyMap& props = base::s_GetProperties();     \
const FunctionMap& funcs = base::s_GetFunctions();      \
s_Api.Properties.insert(                              \
	props.begin(),                                    \
	props.end()                                       \
);                                                    \
s_Api.Functions.insert(                               \
		funcs.begin(),                                \
		funcs.end()                                   \
);                                                    \
}                                                     \
// The following macros (REFLECTION_DECLAREPROP, _DECLAREPROP_SIMPLE and _SIMPLE_READONLY)
// are meant to be called in member functions of Reflection::ReflectionInfo-deriving classes,
// as they use the `s_Api.Properties` member.

// Declare a property with a custom Getter and Setter
#define REFLECTION_DECLAREPROP(strname, type, get, set) s_Api.Properties[strname] = IProperty\
	{	                                                                                     \
		Reflection::ValueType::type,                                                         \
		get,                                                                                 \
		set                                                                                  \
	}                                                                                        \

// Declare a property with the preset Getter (return x) and Setter (x = y)
#define REFLECTION_DECLAREPROP_SIMPLE(c, name, type) REFLECTION_DECLAREPROP(   \
	#name,                                                                     \
	type,                                                                      \
	[](GameObject* p)                                             \
	{                                                                          \
		return (Reflection::GenericValue)dynamic_cast<c*>(p)->name;            \
	},                                                                         \
	[](GameObject* p, Reflection::GenericValue gv)                \
	{                                                                          \
		dynamic_cast<c*>(p)->name = gv.type;                                   \
	}                                                                          \
)                                                                              \

// Declare a property with the preset Getter (return x) and Setter (x = y)
#define REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(c, name, type, cast) REFLECTION_DECLAREPROP(   \
	#name,                                                                     \
	type,                                                                      \
	[](GameObject* p)                                             \
	{                                                                          \
		return (Reflection::GenericValue)dynamic_cast<c*>(p)->name;            \
	},                                                                         \
	[](GameObject* p, Reflection::GenericValue gv)                \
	{                                                                          \
		dynamic_cast<c*>(p)->name = static_cast<cast>(gv.type);                \
	}                                                                          \
)  

// Same as above, but for Complex Types that use Pointer (Vector3 and Color)
// Calls their `operator Reflection::GenericValue`
#define REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(c, name, type) REFLECTION_DECLAREPROP( \
	#name,                                                                              \
	type,                                                                               \
	[](GameObject* p)                                                      \
	{                                                                                   \
		return dynamic_cast<c*>(p)->name.ToGenericValue();                              \
	},                                                                                  \
	[](GameObject* p, Reflection::GenericValue gv)                         \
	{                                                                                   \
		dynamic_cast<c*>(p)->name = type(gv);                                           \
	}                                                                                   \
)                                                                                       \

// Declare a property with the preset Getter, but no Setter
#define REFLECTION_DECLAREPROP_SIMPLE_READONLY(c, name, type) REFLECTION_DECLAREPROP(   \
	#name,                                                                              \
	type,                                                                               \
	[](GameObject* p)                                                      \
	{                                                                                   \
		return Reflection::GenericValue(dynamic_cast<c*>(p)->name);                     \
	},                                                                                  \
	nullptr,                                                                            \
)                                                                                       \

#define REFLECTION_DECLAREFUNC(strname, ins, outs, func) s_Api.Functions[strname] = IFunction \
	{    \
		ins,                                                                                      \
		outs,                                                                                     \
		func                                                                                      \
	}                                                                                             \

#define REFLECTION_DECLAREPROC(strname, func) REFLECTION_DECLAREFUNC(\
	strname,                                                        \
	{},                                                           \
	{},                                                           \
	func                                                          \
)                                                                 \

namespace Reflection
{
	enum class ValueType
	{
		None = 0,

		Bool,
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
		_count
	};

	const std::string& TypeAsString(ValueType);

	struct GenericValue
	{
		Reflection::ValueType Type = Reflection::ValueType::None;
		std::string String;
		bool Bool = true;
		double Double = 0.f;
		int64_t Integer = 0;
		void* Pointer = nullptr;
		std::vector<GenericValue> Array;

		GenericValue();
		GenericValue(std::string const&);
		GenericValue(bool);
		GenericValue(double);
		GenericValue(int);
		GenericValue(int64_t);
		GenericValue(uint32_t);
		GenericValue(const glm::mat4&);

		std::string ToString() const;

		// Throws errors if the type does not match
		std::string AsString() const;
		bool AsBool() const;
		double AsDouble() const;
		int64_t AsInt() const;
		glm::mat4 AsMatrix() const;
	};

	class Reflectable;

	struct IProperty
	{
		Reflection::ValueType Type;

		std::function<Reflection::GenericValue(Reflection::Reflectable*)> Get;
		std::function<void(Reflection::Reflectable*, Reflection::GenericValue&)> Set;
	};

	struct IFunction
	{
		std::vector<ValueType> Inputs;
		std::vector<ValueType> Outputs;

		std::function<GenericValue(Reflection::Reflectable*, GenericValue&)> Func;
	};

	typedef std::unordered_map<std::string, Reflection::IProperty> PropertyMap;
	typedef std::unordered_map<std::string, Reflection::IFunction> FunctionMap;

	class Reflectable
	{
	public:
		Reflectable();
		virtual ~Reflectable() = default;

		static PropertyMap& GetProperties();
		static FunctionMap& GetFunctions();

		static bool HasProperty(const std::string&);
		static bool HasFunction(const std::string&);

		static Reflection::IProperty& GetProperty(const std::string&);
		static Reflection::IFunction& GetFunction(const std::string&);

		GenericValue GetPropertyValue(const std::string&);

		void SetPropertyValue(const std::string&, GenericValue&);
		GenericValue CallFunction(const std::string&, GenericValue);

	protected:
		static inline PropertyMap s_Properties{};
		static inline FunctionMap s_Functions{};
		static inline std::vector<std::string> s_InheritanceTree{ "Reflectable" };
	};
}
