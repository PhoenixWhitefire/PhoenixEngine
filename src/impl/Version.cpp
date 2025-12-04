// Version.hpp, Engine version and Git commit hash

#include "Version.hpp"

#ifndef PHX_VERSION_HPP_ENGINE_GIT_COMMIT
#define PHX_VERSION_HPP_ENGINE_GIT_COMMIT "Build system did not define `PHX_VERSION_HPP_ENGINE_GIT_COMMIT`"
#endif

const char* PhxGetCommitHash()
{
    return PHX_VERSION_HPP_ENGINE_GIT_COMMIT;
}
