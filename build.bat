@echo off
REM ------------------
set vulkanSDKRoot=F:/VulkanSDK/1.2.162.1
set vulkanSDKInclude=%vulkanSDKRoot%\Include
set vulkanSDKLib=%vulkanSDKRoot%\Lib
REM ------------------

set arg0=%1
set buildScriptRoot=%~dp0

REM ------------------
set buildMode=debug
if release==%arg0% (set buildMode=release)
REM ------------------

set thirdPartyDir=%buildScriptRoot%\3rdparty\win64
set sourceDir=%buildScriptRoot%\code
set buildDir=%buildScriptRoot%\build\%buildMode%

if NOT EXIST %buildDir% mkdir %buildDir%

REM ------------------
set debugCompilerFlags=/Od
set releaseCompilerFlags=/O2

set buildModeFlags=%debugCompilerFlags%
if %buildMode%==release (set buildModeFlags=%releaseCompilerFlags%)

set includeFlags=/I "%vulkanSDKInclude%" /I "%sourceDir%" /I "%thirdPartyDir%\inc"
set disableWarnings=/W4 /wd4201 /wd4100 /wd4189 /wd4505
set compilerMacros=/DWIN32 /D_CRT_SECURE_NO_WARNINGS
set compilerFlags=/MTd /nologo /GR- /EHa- /Oi /FC /Z7 /Fe"%buildDir%\gfx.exe" /Fo"%buildDir%\\" %compilerMacros% %disableWarnings% %buildModeFlags% %includeFlags%

set libPathFlags=/LIBPATH:"%vulkanSDKLib%" /LIBPATH:"%thirdPartyDir%\lib\%buildMode%"
set linkLibs=vulkan-1.lib SDL2main.lib SDL2.lib shell32.lib
set linkerFlags=/INCREMENTAL:NO /OPT:REF /SUBSYSTEM:CONSOLE %libPathFlags% %linkLibs%
REM ------------------

cl %compilerFlags% %sourceDir%\main.cpp %thirdPartyDir%/inc/mmgr/mmgr.cpp /link %linkerFlags%

REM ------------------


REM echo %buildMode%
REM echo %buildModeFlags%
REM echo %compilerFlags%