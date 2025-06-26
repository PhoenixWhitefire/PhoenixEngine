// explicit instantiations to reduce compile times

#define VALUEGRADIENT_NO_EXTERN_COMMON

#include "datatype/ValueGradient.hpp"

template class ValueGradientKeypoint<float>;
template class ValueGradientKeypoint<double>;
template class ValueGradientKeypoint<glm::vec2>;
template class ValueGradientKeypoint<glm::vec3>;

template class ValueGradient<float>;
template class ValueGradient<double>;
template class ValueGradient<glm::vec2>;
template class ValueGradient<glm::vec3>;
