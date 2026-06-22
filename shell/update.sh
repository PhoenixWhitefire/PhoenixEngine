#!/bin/bash
set -euo pipefail

git pull
shell/build.sh $@
