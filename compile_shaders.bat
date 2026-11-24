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

echo Compiling triangle shaders...
"%GLSLC%" shaders\triangle.vert -o shaders\triangle.vert.spv
"%GLSLC%" shaders\triangle.frag -o shaders\triangle.frag.spv

echo Compiling postprocess shaders...
"%GLSLC%" shaders\postprocess.vert -o shaders\postprocess.vert.spv
"%GLSLC%" shaders\postprocess.frag -o shaders\postprocess.frag.spv

echo Shaders compiled successfully.
pause