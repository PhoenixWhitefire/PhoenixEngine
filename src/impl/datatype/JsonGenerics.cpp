// JSON and GenericValue, 27/05/2026
#include "datatype/JsonGenerics.hpp"
#include "Utilities.hpp"

// Also in `ScriptEngine.cpp`!!
#define JSON_ENCODED_DATA_TAG "__HX_EncodedData"

Reflection::GenericValue JsonToGeneric(const nlohmann::json& v)
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
            arr.push_back(JsonToGeneric(v[i]));

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
				val = JsonToGeneric(data);

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

nlohmann::json GenericToJson(const Reflection::GenericValue& Value, std::string Context)
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
            arr[i] = GenericToJson(Value.Val.Array[i]);

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

			t[key] = GenericToJson(v, Context + "." + key);
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
