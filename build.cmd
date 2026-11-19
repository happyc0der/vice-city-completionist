@echo off
rem Builds VC100Checklist.asi and VC100Display.exe (32-bit) into .\build
rem Needs the MSVC x86 compiler. Either run this from an "x86 Native Tools Command Prompt",
rem or let it find vcvars32.bat from Visual Studio / Build Tools 2019 or 2022.

setlocal
if /i "%VSCMD_ARG_TGT_ARCH%"=="x86" goto build
for %%E in (BuildTools Community Professional Enterprise) do (
  for %%Y in (2022 2019) do (
    if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars32.bat" call "%ProgramFiles(x86)%\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars32.bat" >nul
    if exist "%ProgramFiles%\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars32.bat" call "%ProgramFiles%\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars32.bat" >nul
  )
)
if /i not "%VSCMD_ARG_TGT_ARCH%"=="x86" (
  echo MSVC x86 environment not found. Install "Build Tools for Visual Studio" with the C++ x86 tools,
  echo or run this from an "x86 Native Tools Command Prompt".
  exit /b 1
)

:build
cd /d "%~dp0"
if not exist build mkdir build
cl /nologo /LD /O2 /MT /EHsc /W3 /D_CRT_SECURE_NO_WARNINGS /Fobuild\ src\VC100Checklist.cpp /Fe:build\VC100Checklist.asi user32.lib || exit /b 1
cl /nologo /O2 /MT /W3 /D_CRT_SECURE_NO_WARNINGS /Fobuild\ src\VC100Display.cpp /Fe:build\VC100Display.exe user32.lib /link /SUBSYSTEM:WINDOWS || exit /b 1
echo.
echo Built build\VC100Checklist.asi and build\VC100Display.exe
