// API Reflection

#pragma once

#include<unordered_map>
#include<functional>
#include<string>
#include<vector>

// The following macros (REFLECTION_DECLAREPROP, _DECLAREPROP_SIMPLE and _SIMPLE_READONLY)
// are meant to be called in member functions of Reflection::Reflectable-deriving classes,
// as they use the `m_properties` member.

// Declare a property with a custom Getter and Setter
#define REFLECTION_DECLAREPROP(name, type, get, set) m_properties.insert(std::pair( \
	#name,                                                                          \
	Reflection::Property                                                            \
	{	                                                                            \
		type,                                                                       \
		get,                                                                        \
		set,                                                                        \
	}                                                                               \
))                                                                                  \

// Declare a property with the preset Getter (return x) and Setter (x = y)
#define REFLECTION_DECLAREPROP_SIMPLE(name, type) REFLECTION_DECLAREPROP( \
	name,                                                                 \
	type,                                                                 \
	[this]()                                                              \
	{                                                                     \
		return name;                                                      \
	},                                                                    \
	[this](Reflection::GenericValue gv)                                   \
	{                                                                     \
		name = gv;                                                    \
	}                                                                     \
)                                                                         \

// Declare a property with the preset Getter, but no Setter
#define REFLECTION_DECLAREPROP_SIMPLE_READONLY(name, type) REFLECTION_DECLAREPROP( \
	name,                                                                          \
	type,                                                                          \
	[this]()                                                                       \
	{                                                                              \
		return name;                                                               \
	},                                                                             \
	nullptr                                                                        \
)                                                                                  \

namespace Reflection
{
	enum class ValueType
	{
		NONE,
		String,
		Bool,
		Double,
		Integer,
		Color,
		Vector3
	};

	struct GenericValue
	{
		ValueType Type = ValueType::NONE;
		std::string String;
		bool Bool = true;
		double Double = 0.f;
		int Integer = 0;
		void* Pointer = nullptr;

		GenericValue();
		GenericValue(std::string);
		GenericValue(bool);
		GenericValue(double);
		GenericValue(int);
		GenericValue(uint32_t);

		std::string ToString();

		operator std::string()
		{
			return this->Type == ValueType::String ? this->String : throw("GenericType was not a String");
		}

		operator bool()
		{
			return this->Type == ValueType::Bool ? this->Bool : throw("GenericType was not a Bool");
		}

		operator double()
		{
			return this->Type == ValueType::Double ? this->Double : throw("GenericType was not a Double");
		}

		operator int()
		{
			return this->Type == ValueType::Integer ? this->Integer : throw("GenericType was not an Integer");
		}

		operator std::vector<GenericValue>()
		{
			auto vec = std::vector<GenericValue>();
			vec.push_back(*this);
			return vec;
		}
	};

	typedef std::vector<GenericValue> GenericValueArray;

	struct Property
	{
		Property
		(
			ValueType,
			std::function<GenericValue(void)>,
			std::function<void(GenericValue)>
		);

		ValueType Type;
		std::function<GenericValue(void)> Getter;
		std::function<void(GenericValue)> Setter;
	};

	struct Function
	{
		Function
		(
			std::vector<ValueType> inputs,
			std::vector<ValueType> outputs,
			std::function<GenericValueArray(GenericValueArray)> func
		);

		std::vector<ValueType> Inputs;
		std::vector<ValueType> Outputs;

		GenericValueArray operator () (GenericValueArray Input)
		{
			return this->Func(Input);
		}

		std::function<GenericValueArray(GenericValueArray)> Func;
	};

	typedef std::unordered_map<std::string, Reflection::Property> PropertyMap;
	typedef std::unordered_map<std::string, Reflection::Function> FunctionMap;

	class Reflectable
	{
	public:
		PropertyMap& GetProperties();
		FunctionMap& GetFunctions();

		bool HasProperty(const std::string&);
		bool HasFunction(const std::string&);

		Reflection::Property& GetProperty(const std::string&);
		Reflection::Function& GetFunction(const std::string&);

		GenericValue GetPropertyValue(const std::string&);

		void SetPropertyValue(const std::string&, GenericValue&);
		GenericValueArray CallFunction(const std::string&, GenericValueArray);

	protected:
		PropertyMap m_properties;
		FunctionMap m_functions;
	};
}
