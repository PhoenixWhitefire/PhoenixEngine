#!/bin/bash
set -euo pipefail

echo Updating your registry
sudo apt-get update

echo Installing libssl-dev
sudo apt-get install libssl-dev

echo Installing libwayland-dev
sudo apt-get install libwayland-dev

echo Installing libwayland-bin
sudo apt-get install libwayland-bin

echo Installing wayland-protocols
sudo apt-get install wayland-protocols

echo Installing libxkbcommon-dev
sudo apt-get install libxkbcommon-dev

echo Installing xorg-dev
sudo apt-get install xorg-dev

echo Installing libdbus-1-dev
sudo apt-get install libdbus-1-dev

echo Installing Mesa and Wayland GL
sudo apt-get install libegl1-mesa-dev libgl1-mesa-dev libwayland-egl1

echo Installing Wayland client
sudo apt-get install libwayland-client0

echo Installing clang
sudo apt-get install clang

echo Installing lld
sudo apt-get install lld
