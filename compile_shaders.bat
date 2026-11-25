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
"%GLSLC%" shaders\triangle.vert -o shaders\triangle.vert.spv || exit /b 1
"%GLSLC%" shaders\triangle.frag -o shaders\triangle.frag.spv || exit /b 1

echo Compiling postprocess shaders...
"%GLSLC%" shaders\postprocess.vert -o shaders\postprocess.vert.spv || exit /b 1
"%GLSLC%" shaders\postprocess.frag -o shaders\postprocess.frag.spv || exit /b 1

echo Compiling waterfall shaders...
"%GLSLC%" shaders\waterfall.vert -o shaders\waterfall.vert.spv || exit /b 1
"%GLSLC%" shaders\waterfall.frag -o shaders\waterfall.frag.spv || exit /b 1

echo Compiling worm shaders...
"%GLSLC%" shaders\worm.vert -o shaders\worm.vert.spv || exit /b 1
"%GLSLC%" shaders\worm.frag -o shaders\worm.frag.spv || exit /b 1

echo Shaders compiled successfully.
pause
