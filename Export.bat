@echo OFF

echo 22/11/2024
echo "The finest batch scripts"
echo This will create a "Build" directory if it does not exist (clearing it if it does), and create a minimum environment for the Engine, ready for distribution (other than the "resources/models" folder not being deleted)
echo Proceed at your own risk

pause

echo Removing previous build
del Build

mkdir "Build"

echo Copying resources
xcopy "resources" "Build/resources" /s /e /i
echo Copying configuration
copy "phoenix.conf" "Build/phoenix.conf"
echo Copying executable
copy "x64\Release\PhoenixEngine.exe" "Build/Demo.exe"
echo Copying DLLs
copy "SDL2.dll" "Build\SDL2.dll"

echo Deleting non-cooked models
del "Build\resources\models"