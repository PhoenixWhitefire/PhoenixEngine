// Version.hpp, Engine version and Git commit hash

#include "Version.hpp"

#define PHX_VERSION "Prototype 2026.1+"

const char* GetEngineVersion()
{
    return PHX_VERSION;
}

#ifndef PHX_VERSION_HPP_ENGINE_GIT_COMMIT
#define PHX_VERSION_HPP_ENGINE_GIT_COMMIT "Unknown(e=2)"
#endif

#ifndef PHX_VERSION_HPP_ENGINE_GIT_COMMIT_TAG
#define PHX_VERSION_HPP_ENGINE_GIT_COMMIT_TAG "Unknown(e=2)"
#endif

const char* GetEngineCommitHash()
{
    return PHX_VERSION_HPP_ENGINE_GIT_COMMIT;
}

const char* GetEngineCommitTag()
{
    return PHX_VERSION_HPP_ENGINE_GIT_COMMIT_TAG;
}

const char* GetEngineBuildDate()
{
    return __DATE__;
}

const char* GetEngineBuildTime()
{
    return __TIME__;
}
