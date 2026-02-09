@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "BUILD_DIR=%ROOT%\build"
set "CFG=%~1"
if "%CFG%"=="" set "CFG=Release"
set "ACTION=%~2"
set "GEN=%~3"

if /I "%ACTION%"=="clean" (
  if exist "%BUILD_DIR%" rmdir /S /Q "%BUILD_DIR%"
)

set "CMAKE_EXE=cmake"
where cmake >nul 2>nul
if errorlevel 1 (
  set "CMAKE_EXE="
  set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
  if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSROOT=%%i"
    if defined VSROOT (
      if exist "%VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
        set "CMAKE_EXE=%VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
      )
    )
  )
  if not defined CMAKE_EXE (
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
      set "CMAKE_EXE=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )
  )
  if not defined CMAKE_EXE (
    echo cmake not found in PATH and Visual Studio CMake not found.
    exit /b 2
  )
)

if "%GEN%"=="" (
  set "GEN=Visual Studio 16 2019"
)

"%CMAKE_EXE%" -S "%ROOT%" -B "%BUILD_DIR%" -G "%GEN%" -A x64
if errorlevel 1 (
  if "%~3"=="" (
    set "GEN=Visual Studio 17 2022"
    "%CMAKE_EXE%" -S "%ROOT%" -B "%BUILD_DIR%" -G "%GEN%" -A x64
  )
)
if errorlevel 1 (
  echo CMake configure failed.
  exit /b 3
)

"%CMAKE_EXE%" --build "%BUILD_DIR%" --config "%CFG%" -m
if errorlevel 1 (
  echo Build failed.
  exit /b 4
)

set "OUT=%BUILD_DIR%\bin\%CFG%\AutoClickerPro.exe"
if not exist "%OUT%" set "OUT=%BUILD_DIR%\%CFG%\AutoClickerPro.exe"
if exist "%OUT%" (
  echo Build OK: %OUT%
  exit /b 0
)

echo Build OK, but output exe not found in expected locations.
exit /b 5
