// JSON and GenericValue, 27/05/2026
#pragma once

#include <nljson.hpp>
#include "Reflection.hpp"

Reflection::GenericValue JsonToGeneric(const nlohmann::json& v);
nlohmann::json GenericToJson(const Reflection::GenericValue& Value, std::string Context = "");
