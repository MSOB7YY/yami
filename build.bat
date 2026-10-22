@echo off
setlocal
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%i"
if not defined VS_PATH (
  echo Visual Studio C++ tools not found
  exit /b 1
)
call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 || exit /b 1
cd /d "%~dp0"
if not exist build\obj mkdir build\obj
rc /nologo /fo build\obj\app.res res\app.rc || exit /b 1
cl /nologo /std:c++20 /O2 /GL /MT /MP /EHsc /W4 /permissive- /utf-8 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Fobuild\obj\ /Febuild\Yami.exe src\*.cpp build\obj\app.res /link /LTCG /SUBSYSTEM:WINDOWS user32.lib gdi32.lib dxva2.lib shell32.lib advapi32.lib dwmapi.lib shcore.lib wtsapi32.lib
