#include "component/TreeLink.hpp"

class TreeLinkManager : public ComponentManager<EcTreeLink>
{
};
static TreeLinkManager Instance{};
