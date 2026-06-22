#!/bin/bash
set -euo pipefail

buildConfig="Release"

if [ -z "$@" ]; then
    echo "The build configuration is defaulting to $buildConfig"
else
    buildConfig=$@
    echo "The build configuration is $buildConfig"
fi

shell/configure.sh
cmake --build build --config $buildConfig -j7
