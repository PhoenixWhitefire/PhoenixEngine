// DecomposeTRS.cpp, 27/06/2026
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/matrix_decompose.hpp"

#include "geometry/DecomposeTRS.hpp"

bool DecomposeTRS(const glm::mat4& Matrix, glm::vec3* Translation, glm::quat* Rotation, glm::vec3* Scale)
{
    glm::vec3 scale = {};
    glm::quat rotation = {};
    glm::vec3 translation = {};
    glm::vec3 skew = {};
    glm::vec4 perspective = {};

    bool result = glm::decompose(Matrix, scale, rotation, translation, skew, perspective);

    if (Translation)
        *Translation = translation;

    if (Rotation)
        *Rotation = rotation;
    
    if (Scale)
        *Scale = scale;

    return result;
}
