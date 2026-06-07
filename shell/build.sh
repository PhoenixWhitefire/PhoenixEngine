#!/bin/bash
set -euo pipefail

shell/configure.sh
cmake --build build --config $@ -j7
