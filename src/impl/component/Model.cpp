#include "component/Model.hpp"
#include "datatype/GameObject.hpp"

class ModelManager : public ComponentManager<EcModel>
{
};

static inline ModelManager Instance;
