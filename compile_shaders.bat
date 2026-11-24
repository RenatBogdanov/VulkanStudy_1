@echo off
if "%VULKAN_SDK%"=="" (
    echo Error: VULKAN_SDK environment variable is not set.
    exit /b 1
)

if not exist "shaders\triangle.vert" (
    echo Error: run this script from the project root directory.
    exit /b 1
)

set GLSLC=%VULKAN_SDK%\Bin\glslc.exe
if not exist "%GLSLC%" (
    echo Error: glslc.exe not found at %GLSLC%
    exit /b 1
)

echo Compiling vertex shader...
"%GLSLC%" shaders\triangle.vert -o shaders\triangle.vert.spv
if errorlevel 1 exit /b 1

echo Compiling fragment shader...
"%GLSLC%" shaders\triangle.frag -o shaders\triangle.frag.spv
if errorlevel 1 exit /b 1

echo Shaders compiled successfully.