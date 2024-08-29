// API Reflection

#pragma once

#include<unordered_map>
#include<functional>
#include<string>
#include<vector>
#include<format>

#define REFLECTION_OPERATORGENERICTOCOMPLEX(type) Reflection::GenericValue gv;                \
gv.Type = Reflection::ValueType::type;                                                        \
gv.Pointer = malloc(sizeof(type));                                                            \
if (!gv.Pointer)                                                                              \
throw(std::vformat(                                                                           \
	"Allocation error while casting {} to Reflection::GenericValue (malloc nullptr)",         \
	std::make_format_args(#type)                                                              \
));                                                                                           \
memcpy(gv.Pointer, this, sizeof(type));                                                       \
return gv                                                                                     \

#define REFLECTION_INHERITAPI(base) ApiReflection->s_Properties.insert( \
	base::ApiReflection->s_Properties.begin(),                          \
	base::ApiReflection->s_Properties.end()                             \
);                                                                      \
ApiReflection->s_Functions.insert(                                      \
		base::ApiReflection->s_Functions.begin(),                       \
		base::ApiReflection->s_Functions.end()                          \
);                                                                      \

// The following macros (REFLECTION_DECLAREPROP, _DECLAREPROP_SIMPLE and _SIMPLE_READONLY)
// are meant to be called in member functions of Reflection::ReflectionInfo-deriving classes,
// as they use the `s_Properties` member.

// Declare a property with a custom Getter and Setter
#define REFLECTION_DECLAREPROP(strname, type, get, set) ApiReflection->s_Properties.insert(std::pair(    \
	strname,                                                                                 \
	Reflection::IProperty                                                                 \
	{	                                                                                     \
		Reflection::ValueType::type,                                                         \
		get,                                                                                 \
		set                                                                                  \
	}                                                                                        \
))                                                                                           \

// Declare a property with the preset Getter (return x) and Setter (x = y)
#define REFLECTION_DECLAREPROP_SIMPLE(c, name, type) REFLECTION_DECLAREPROP(   \
	#name,                                                                     \
	type,                                                                      \
	[](Reflection::BaseReflectable* p)                                             \
	{                                                                          \
		return (Reflection::GenericValue)dynamic_cast<c*>(p)->name;            \
	},                                                                         \
	[](Reflection::BaseReflectable* p, Reflection::GenericValue gv)                \
	{                                                                          \
		dynamic_cast<c*>(p)->name = gv.type;                                   \
	}                                                                          \
)                                                                              \

// Declare a property with the preset Getter (return x) and Setter (x = y)
#define REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(c, name, type, cast) REFLECTION_DECLAREPROP(   \
	#name,                                                                     \
	type,                                                                      \
	[](Reflection::BaseReflectable* p)                                             \
	{                                                                          \
		return (Reflection::GenericValue)dynamic_cast<c*>(p)->name;            \
	},                                                                         \
	[](Reflection::BaseReflectable* p, Reflection::GenericValue gv)                \
	{                                                                          \
		dynamic_cast<c*>(p)->name = static_cast<cast>(gv.type);                \
	}                                                                          \
)  

// Same as above, but for Complex Types that use Pointer (Vector3 and Color)
// Calls their `operator Reflection::GenericValue`
#define REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(c, name, type) REFLECTION_DECLAREPROP( \
	#name,                                                                              \
	type,                                                                               \
	[](Reflection::BaseReflectable* p)                                                      \
	{                                                                                   \
		return dynamic_cast<c*>(p)->name.ToGenericValue();                              \
	},                                                                                  \
	[](Reflection::BaseReflectable* p, Reflection::GenericValue gv)                         \
	{                                                                                   \
		dynamic_cast<c*>(p)->name = type(gv);                                           \
	}                                                                                   \
)                                                                                       \

// Declare a property with the preset Getter, but no Setter
#define REFLECTION_DECLAREPROP_SIMPLE_READONLY(c, name, type) REFLECTION_DECLAREPROP(   \
	#name,                                                                              \
	type,                                                                               \
	[](Reflection::BaseReflectable* p)                                                      \
	{                                                                                   \
		return Reflection::GenericValue(dynamic_cast<c*>(p)->name);                     \
	},                                                                                  \
	nullptr,                                                                            \
)                                                                                       \

#define REFLECTION_DECLAREFUNC(strname, ins, outs, func) ApiReflection->s_Functions.insert(std::pair(strname, \
	Reflection::IFunction(                                                                     \
		ins,                                                                                      \
		outs,                                                                                     \
		func                                                                                      \
	)                                                                                             \
))                                                                                                \

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
		String,
		Bool,
		Double,
		Integer,
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

	std::string TypeAsString(ValueType);

	struct GenericValue
	{
		Reflection::ValueType Type = Reflection::ValueType::None;
		std::string String;
		bool Bool = true;
		double Double = 0.f;
		int Integer = 0;
		void* Pointer = nullptr;
		std::vector<GenericValue> Array;

		GenericValue();
		GenericValue(std::string const&);
		GenericValue(bool);
		GenericValue(double);
		GenericValue(int);
		GenericValue(uint32_t);

		std::string ToString() const;

		// Throws errors if the type does not match
		std::string AsString() const;
		bool AsBool() const;
		double AsDouble() const;
		int AsInt() const;
	};

	class BaseReflectable
	{
	public:
		virtual void pointlessPolymorhpismRequirementMeeter();
	};

	struct IProperty
	{
		IProperty
		(
			Reflection::ValueType,
			std::function<Reflection::GenericValue(Reflection::BaseReflectable*)>,
			std::function<void(Reflection::BaseReflectable*, Reflection::GenericValue)>
		);

		IProperty
		(
			Reflection::ValueType,
			bool,
			std::function<Reflection::GenericValue(Reflection::BaseReflectable*)>,
			std::function<void(Reflection::BaseReflectable*, Reflection::GenericValue)>
		);

		Reflection::ValueType Type;
		bool Settable = true;

		std::function<Reflection::GenericValue(Reflection::BaseReflectable*)> Get;
		std::function<void(Reflection::BaseReflectable*, Reflection::GenericValue)> Set;
	};

	struct IFunction
	{
		IFunction
		(
			std::vector<ValueType> inputs,
			std::vector<ValueType> outputs,
			std::function<GenericValue(Reflection::BaseReflectable*, GenericValue)> func
		);

		std::vector<ValueType> Inputs;
		std::vector<ValueType> Outputs;

		std::function<GenericValue(Reflection::BaseReflectable*, GenericValue)> Func;
	};

	struct ReflectionInfo
	{
		typedef std::unordered_map<std::string, Reflection::IProperty> PropertyMap;
		typedef std::unordered_map<std::string, Reflection::IFunction> FunctionMap;

		virtual PropertyMap& GetProperties();
		virtual FunctionMap& GetFunctions();

		virtual bool HasProperty(const std::string&);
		virtual bool HasFunction(const std::string&);

		virtual Reflection::IProperty& GetProperty(const std::string&);
		virtual Reflection::IFunction& GetFunction(const std::string&);

		virtual GenericValue GetPropertyValue(const std::string&);

		virtual void SetPropertyValue(const std::string&, GenericValue&);
		virtual GenericValue CallFunction(const std::string&, GenericValue);

		PropertyMap s_Properties;
		FunctionMap s_Functions;
		bool s_DidInitReflection = false;
	};

	class Reflectable : public BaseReflectable
	{
	public:
		Reflectable();

		static Reflection::ReflectionInfo* ApiReflection;
	};
}
