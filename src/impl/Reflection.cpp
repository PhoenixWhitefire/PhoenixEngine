#include"Reflection.hpp"
#include"datatype/Vector3.hpp"
#include"datatype/Color.hpp"
#include"datatype/GameObject.hpp"

//Reflection::Reflectable* Reflection::Reflectable::ApiReflection = new Reflection::Reflectable();

Reflection::GenericValue::GenericValue()
	: Type(ValueType::Null)
{
}

Reflection::GenericValue::GenericValue(std::string const& str)
	: Type(ValueType::String), String(str)
{
}

Reflection::GenericValue::GenericValue(bool b)
	: Type(ValueType::Bool), Pointer((void*)b)
{
}

Reflection::GenericValue::GenericValue(double d)
	: Type(ValueType::Double), Pointer(*(void**)&d)
{
}

Reflection::GenericValue::GenericValue(int i)
	: Type(ValueType::Integer), Pointer((void*)i)
{
}

Reflection::GenericValue::GenericValue(int64_t i)
	: Type(ValueType::Integer), Pointer((void*)i)
{
}

Reflection::GenericValue::GenericValue(uint32_t i)
	: Type(ValueType::Integer), Pointer((void*)i)
{
}

Reflection::GenericValue::GenericValue(const glm::mat4& m)
	: Type(ValueType::Matrix), Pointer(malloc(sizeof(m)))
{
	if (!Pointer)
		throw("Allocation error while constructing GenericValue from glm::mat4");

	memcpy(Pointer, &m, sizeof(m));
}

std::string Reflection::GenericValue::ToString() const
{
	switch (this->Type)
	{
	case (ValueType::Null):
		return "Null";

	case (ValueType::Bool):
		return (bool)this->Pointer ? "true" : "false";

	case (ValueType::Integer):
		return std::to_string((int64_t)this->Pointer);

	case (ValueType::Double):
		return std::to_string(*(double*)&this->Pointer);

	case (ValueType::String):
		return this->String;
	
	case (ValueType::Color):
		return Color(*this).ToString();

	case (ValueType::Vector3):
		return Vector3(*this).ToString();

	case (ValueType::GameObject):
	{
		GameObject* object = GameObject::GetObjectById(*(uint32_t*)&this->Pointer);
		return object ? object->GetFullName() : "NULL GameObject";
	}
	
	case (ValueType::Array):
	{
		if (this->Array.size() > 0)
		{
			uint32_t numTypes = 0;
			std::string typesString = "";

			for (const Reflection::GenericValue& element : this->Array)
			{
				numTypes++;
				if (numTypes > 4)
				{
					typesString += "...|";
					break;
				}
				else
					typesString += Reflection::TypeAsString(element.Type) + "|";
			}

			typesString = typesString.substr(0, typesString.size() - 1);

			return std::vformat("Array<{}>", std::make_format_args(typesString));
		}
		else
			return "Empty Array";

	case (ValueType::Map):
	{
		if (this->Array.size() > 0)
		{
			if (this->Array.size() % 2 != 0)
				return "Invalid Map (Odd number of Array elements)";

			return std::vformat(
				"Map<{}:{}>",
				std::make_format_args(
					Reflection::TypeAsString(this->Array[0].Type),
					Reflection::TypeAsString(this->Array[1].Type)
				));
		}
		else
			return "Empty Map";
	}
	}

	default:
	{
		std::string tName = TypeAsString(Type);
		return std::vformat(
			"GenericValue::ToString failed, Type was: {}",
			std::make_format_args(tName)
		);
	}
	}
}

std::string Reflection::GenericValue::AsString() const
{
	return Type == ValueType::String
		? String : throw("GenericValue was not a String, but was a " + Reflection::TypeAsString(Type));
}
bool Reflection::GenericValue::AsBool() const
{
	return Type == ValueType::Bool
		? (bool)this->Pointer
		: throw("GenericValue was not a Bool, but was a " + Reflection::TypeAsString(Type));
}
double Reflection::GenericValue::AsDouble() const
{
	return Type == ValueType::Double
		? *(double*)&this->Pointer
		: throw("GenericValue was not a Double, but was a " + Reflection::TypeAsString(Type));
}
int64_t Reflection::GenericValue::AsInteger() const
{
	// `|| ValueType::GameObject` because it's easier 14/09/2024
	return (Type == ValueType::Integer || Type == ValueType::GameObject)
		? (int64_t)this->Pointer
		: throw("GenericValue was not an Integer, but was a " + Reflection::TypeAsString(Type));
}
glm::mat4 Reflection::GenericValue::AsMatrix() const
{
	glm::mat4* mptr = (glm::mat4*)Pointer;

	return Type == ValueType::Matrix
		? *mptr
		: throw("GenericValue was not a Matrix, but was a " + Reflection::TypeAsString(Type));
}

Reflection::PropertyMap& Reflection::Reflectable::GetProperties()
{
	return  s_Properties;
}

Reflection::FunctionMap& Reflection::Reflectable::GetFunctions()
{
	return  s_Functions;
}

bool Reflection::Reflectable::HasProperty(const std::string& name)
{
	if ( s_Properties.size() == 0)
		return false;
	return  s_Properties.find(name) !=  s_Properties.end();
}

bool Reflection::Reflectable::HasFunction(const std::string& name)
{
	return  s_Functions.find(name) !=  s_Functions.end();
}

Reflection::IProperty& Reflection::Reflectable::GetProperty(const std::string& name)
{
	if (!HasProperty(name))
		throw(std::vformat("(GetProperty) No Property named {}", std::make_format_args(name)));
	else
		return  s_Properties.find(name)->second;
}

Reflection::IFunction& Reflection::Reflectable::GetFunction(const std::string& name)
{
	if (!HasFunction(name))
		throw(std::vformat("(GetFunction) No Function named {}", std::make_format_args(name)));
	else
		return  s_Functions.find(name)->second;
}

Reflection::GenericValue Reflection::Reflectable::GetPropertyValue(const std::string& name)
{
	if (!HasProperty(name))
		throw(std::vformat("(GetPropertyValue) No Property named {}", std::make_format_args(name)));
	else
		return  s_Properties.find(name)->second.Get(dynamic_cast<Reflection::Reflectable*>(this));
}

void Reflection::Reflectable::SetPropertyValue(const std::string& name, GenericValue& value)
{
	if (!HasProperty(name))
		throw(std::vformat("(SetPropertyValue) No Property named {}", std::make_format_args(name)));
	else
		 s_Properties.find(name)->second.Set((Reflection::Reflectable*)this, value);
}

Reflection::GenericValue Reflection::Reflectable::CallFunction(const std::string& name, GenericValue input)
{
	if (!HasFunction(name))
		throw(std::vformat("(CallFunction) No Function named {}", std::make_format_args(name)));
	else
		return  s_Functions.find(name)->second.Func((Reflection::Reflectable*)this, input);
}

static bool s_DidInitReflection = false;

Reflection::Reflectable::Reflectable()
{
	if (s_DidInitReflection)
		return;
	s_DidInitReflection = true;

	//Reflectable::ApiReflection = new Reflection::Reflectable();
	
}

static std::string valueTypeNames[] =
{
		"Null",

		"Bool",
		"Integer",
		"Double",
		"String",

		"Color",
		"Vector3",
		"Matrix",

		"GameObject",

		"Array",
		"Map"
};

static_assert(
	std::size(valueTypeNames) == (static_cast<uint32_t>(Reflection::ValueType::_count)),
	"`ValueTypeNames` does not have the same number of elements as `ValueType`"
);

const std::string& Reflection::TypeAsString(ValueType t)
{
	return valueTypeNames[(int)t];
}
