// Version.hpp, Engine version and Git commit hash

#include "Version.hpp"

#define PHX_VERSION "Prototype W.I.P."

const char* GetEngineVersion()
{
    return PHX_VERSION;
}

#ifndef PHX_VERSION_HPP_ENGINE_GIT_COMMIT
#define PHX_VERSION_HPP_ENGINE_GIT_COMMIT "Build system did not define `PHX_VERSION_HPP_ENGINE_GIT_COMMIT`"
#endif

const char* GetEngineCommitHash()
{
    return PHX_VERSION_HPP_ENGINE_GIT_COMMIT;
}

const char* GetEngineBuildDate()
{
    return __DATE__;
}

const char* GetEngineBuildTime()
{
    return __TIME__;
}
