#!/bin/bash
bash shell/l_chmod.sh
shell/l_dependencies.sh
shell/l_configure.sh
shell/l_build.sh $@
