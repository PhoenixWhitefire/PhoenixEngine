// Renderer service, 29/01/2026
#include "component/RendererService.hpp"

class RendererServiceManager : public ComponentManager<EcRendererService>
{
};

static RendererServiceManager Instance;
