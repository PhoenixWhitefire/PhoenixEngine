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
	[](Reflection::Reflectable* p)                                             \
	{                                                                          \
		return (Reflection::GenericValue)static_cast<c*>(p)->name;             \
	},                                                                         \
	[](Reflection::Reflectable* p, const Reflection::GenericValue& gv)         \
	{                                                                          \
		static_cast<c*>(p)->name = gv.As##type();                              \
	}                                                                          \
)                                                                              \

// Declare a property with the preset Getter (return x) and Setter (x = y),
// statically-casting `y` into `cast`. Useful for getting rid of "possible loss of data"
// warnings when converting between similar types (like `double` and `float`)
#define REFLECTION_DECLAREPROP_SIMPLE_STATICCAST(c, name, type, cast) REFLECTION_DECLAREPROP(   \
	#name,                                                                                      \
	type,                                                                                       \
	[](Reflection::Reflectable* p)                                                              \
	{                                                                                           \
		return (Reflection::GenericValue)static_cast<c*>(p)->name;                              \
	},                                                                                          \
	[](Reflection::Reflectable* p, const Reflection::GenericValue& gv)                          \
	{                                                                                           \
		static_cast<c*>(p)->name = static_cast<cast>(gv.As##type());                            \
	}                                                                                           \
)  

// Same as above, but for Complex Types that use Pointer (Vector3 and Color)
// Calls their `operator Reflection::GenericValue`
#define REFLECTION_DECLAREPROP_SIMPLE_TYPECAST(c, name, type) REFLECTION_DECLAREPROP(   \
	#name,                                                                              \
	type,                                                                               \
	[](Reflection::Reflectable* p)                                                      \
	{                                                                                   \
		return static_cast<c*>(p)->name.ToGenericValue();                               \
	},                                                                                  \
	[](Reflection::Reflectable* p, const Reflection::GenericValue& gv)                  \
	{                                                                                   \
		static_cast<c*>(p)->name = type(gv);                                            \
	}                                                                                   \
)                                                                                       \

// Declare a property with the preset Getter, but no Setter
#define REFLECTION_DECLAREPROP_SIMPLE_READONLY(c, name, type) REFLECTION_DECLAREPROP(   \
	#name,                                                                              \
	type,                                                                               \
	[](Reflection::Reflectable* p)                                                      \
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
	[name##Lambda](Reflection::Reflectable* p, const Reflection::GenericValue&)              \
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
static const std::vector<std::string>& s_GetLineage()                                  \
{                                                                                      \
	return s_Api.Lineage;                                                              \
}                                                                                      \

namespace Reflection
{
	enum class ValueType : uint8_t
	{
		Null = 0,

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
		Reflection::ValueType Type = Reflection::ValueType::Null;
		void* Value = nullptr;
		// Array length or allocated memory for `Value`
		size_t Size = 0;

		//std::vector<GenericValue> Array;

		GenericValue();
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

		GenericValue(GenericValue&&);
		GenericValue(const GenericValue&);
		
		GenericValue& operator = (const Reflection::GenericValue& Other)
		{
			CopyInto(*this, Other);
			return *this;
		}

		static void CopyInto(GenericValue&, const GenericValue&);

		~GenericValue();

		std::string ToString();

		// Throws errors if the type does not match
		std::string AsString() const;
		bool AsBool() const;
		double AsDouble() const;
		int64_t AsInteger() const;
		glm::mat4& AsMatrix() const;
		std::vector<GenericValue> AsArray() const;
		std::unordered_map<GenericValue, GenericValue> AsMap() const;
	};

	class Reflectable;

	struct Property
	{
		Reflection::ValueType Type{};

		std::function<Reflection::GenericValue(Reflection::Reflectable*)> Get;
		std::function<void(Reflection::Reflectable*, const Reflection::GenericValue&)> Set;
	};

	struct Function
	{
		std::vector<ValueType> Inputs;
		std::vector<ValueType> Outputs;

		std::function<std::vector<Reflection::GenericValue>(Reflection::Reflectable*, const std::vector<Reflection::GenericValue>&)> Func;
	};

	typedef std::unordered_map<std::string, Reflection::Property> PropertyMap;
	typedef std::unordered_map<std::string, Reflection::Function> FunctionMap;

	struct Api
	{
		PropertyMap Properties;
		FunctionMap Functions;
		std::vector<std::string> Lineage;
	};

	class Reflectable
	{
	public:
		virtual const Reflection::PropertyMap& GetProperties() const
		{
			return ApiPointer->Properties;
		}
		virtual const Reflection::FunctionMap& GetFunctions() const
		{
			return ApiPointer->Functions;
		}
		virtual const std::vector<std::string>& GetLineage() const
		{
			return ApiPointer->Lineage;
		}
		virtual bool HasProperty(const std::string& MemberName) const
		{
			return ApiPointer->Properties.find(MemberName) != ApiPointer->Properties.end();
		}
		virtual bool HasFunction(const std::string& MemberName) const
		{
			return ApiPointer->Functions.find(MemberName) != ApiPointer->Functions.end();
		}
		virtual const Reflection::Property& GetProperty(const std::string& MemberName) const
		{
			return HasProperty(MemberName)
			? ApiPointer->Properties[MemberName]
			: throw(std::string("Invalid Property in GetProperty ") + MemberName);
		}
		virtual const Reflection::Function& GetFunction(const std::string& MemberName) const
		{
			return HasFunction(MemberName)
			? ApiPointer->Functions[MemberName]
			: throw(std::string("Invalid Function in GetFunction ") + MemberName);
		}
		virtual Reflection::GenericValue GetPropertyValue(const std::string& MemberName)
		{
			return HasProperty(MemberName)
			? ApiPointer->Properties[MemberName].Get(this)
			: throw(std::string("Invalid Property in GetPropertyValue ") + MemberName);
		}
		virtual void SetPropertyValue(const std::string& MemberName, const Reflection::GenericValue& NewValue)
		{
			if (HasProperty(MemberName))
				if (ApiPointer->Properties[MemberName].Set)
					ApiPointer->Properties[MemberName].Set(this, NewValue);
				else
					throw(std::vformat(
						"Tried to set read-only property '{}' via `::SetPropertyValue`",
						std::make_format_args(MemberName)
					));
			else
				throw(std::string("Invalid Property in SetPropertyValue ") + MemberName);
		}
		virtual Reflection::GenericValue CallFunction(const std::string& MemberName, const std::vector<Reflection::GenericValue>& Param)
		{
			if (HasFunction(MemberName))
				return ApiPointer->Functions[MemberName].Func(this, Param);
			else
				throw(std::string("Invalid Function in CallFunction " + MemberName)); \
		}

		REFLECTION_DECLAREAPI;
	
	protected:
		Reflection::Api* ApiPointer = &s_Api;

	private:
		static inline Reflection::Api s_Api{};;
	};
}
