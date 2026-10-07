@echo off
rem Usage: build.bat [okx-debug|okx-release|okx-relwithdebinfo]
setlocal
set PRESET=%1
if "%PRESET%"=="" set PRESET=okx-relwithdebinfo
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath`) do set VSPATH=%%i
rem vcvars runs vswhere by name, so make sure its folder is on PATH.
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if not defined INCLUDE (echo Visual Studio environment failed to load. & exit /b 1)
rem Use Visual Studio's own CMake and clang, not other installs that may be first on PATH.
set "PATH=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VSPATH%\VC\Tools\Llvm\x64\bin;%PATH%"
cd /d "%~dp0"
cmake --preset %PRESET% || exit /b 1
cmake --build --preset %PRESET% || exit /b 1
