#!/bin/bash

cd Vendor/tracy/profiler
cmake -B build -G Ninja
cmake --build build --config Release -j7
