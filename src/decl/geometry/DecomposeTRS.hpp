// DecomposeTRS.hpp, 27/06/2026
#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

bool DecomposeTRS(const glm::mat4& Matrix, glm::vec3* Translation = nullptr, glm::quat* Rotation = nullptr, glm::vec3* Scale = nullptr);
