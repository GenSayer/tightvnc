@echo off
rem build.cmd [i386|ALPHA|ARM] [debug] [staticcrt] [visualstyles] [comctl4|5|6]
rem           [minihelp|htmlhelp|nohelp]
rem
rem Builds vncviewer and WinVNC (plus VNCHooks.dll) from the command line.
rem Run from the vnc_winsrc directory with the compiler environment for the
rem target CPU set up (PATH, INCLUDE, LIB): Visual C++ 4.x - 6.0 for i386 or
rem Alpha, a Visual Studio 2015 ARM environment (for example vcvarsall.bat
rem x86_arm) for ARM.  With no CPU argument, PROCESSOR_ARCHITECTURE picks it.
rem "staticcrt" links the static CRT into VNCHooks.dll instead of the DLL CRT.
rem Output: <dir>\Rel_<CPU> or <dir>\Dbg_<CPU>.
rem
rem "comctl4", "comctl5" or "comctl6" compiles for that Common Controls level
rem (_WIN32_IE; "comctl6" implies "visualstyles", see nt.inc).
rem "visualstyles" embeds a Common Controls 6 manifest, so the programs use the
rem current theme on Windows XP and later (see nt.inc).
rem
rem "minihelp" (the default), "htmlhelp" or "nohelp" picks the context help
rem of the dialogs: the built-in text popups, the original HtmlHelp popups
rem (links htmlhelp.lib, which must match the CPU), or none (see nt.inc).
rem
rem The Developer Studio files (WinVNC.mak, vncviewer.mak, *.dsp, *.vcproj)
rem still work as before; this script uses the nt.mak files instead.
rem
rem Written for the NT 3.5 command interpreter: no "call :label", no pushd,
rem no "if /i".

set NTCPU=%1
if "%NTCPU%"=="" set NTCPU=%PROCESSOR_ARCHITECTURE%
if "%NTCPU%"=="x86" set NTCPU=i386
if "%NTCPU%"=="X86" set NTCPU=i386
if "%NTCPU%"=="I386" set NTCPU=i386
if "%NTCPU%"=="alpha" set NTCPU=ALPHA
if "%NTCPU%"=="Alpha" set NTCPU=ALPHA
if "%NTCPU%"=="AXP" set NTCPU=ALPHA
if "%NTCPU%"=="arm" set NTCPU=ARM
if "%NTCPU%"=="Arm" set NTCPU=ARM
if "%NTCPU%"=="i386" goto cpuok
if "%NTCPU%"=="ALPHA" goto cpuok
if "%NTCPU%"=="ARM" goto cpuok
echo Unknown CPU "%NTCPU%".  Use i386, ALPHA or ARM.
goto end
:cpuok

set NTARGS=CPU=%NTCPU%
set NTOUT=Rel_%NTCPU%
set NTDBG=
set NTVS=
set NTCC=
set NTSTAT=
set NTHELP=

rem Options after the CPU, in any order.
:opts
shift
if "%1"=="" goto optsdone
if "%1"=="debug" set NTDBG=1
if "%1"=="DEBUG" set NTDBG=1
if "%1"=="Debug" set NTDBG=1
if "%1"=="staticcrt" set NTSTAT=1
if "%1"=="STATICCRT" set NTSTAT=1
if "%1"=="Staticcrt" set NTSTAT=1
if "%1"=="visualstyles" set NTVS=1
if "%1"=="VISUALSTYLES" set NTVS=1
if "%1"=="Visualstyles" set NTVS=1
if "%1"=="comctl4" set NTCC=4
if "%1"=="comctl5" set NTCC=5
if "%1"=="comctl6" set NTCC=6
if "%1"=="COMCTL4" set NTCC=4
if "%1"=="COMCTL5" set NTCC=5
if "%1"=="COMCTL6" set NTCC=6
if "%1"=="minihelp" set NTHELP=mini
if "%1"=="MINIHELP" set NTHELP=mini
if "%1"=="Minihelp" set NTHELP=mini
if "%1"=="htmlhelp" set NTHELP=html
if "%1"=="HTMLHELP" set NTHELP=html
if "%1"=="Htmlhelp" set NTHELP=html
if "%1"=="nohelp" set NTHELP=none
if "%1"=="NOHELP" set NTHELP=none
if "%1"=="Nohelp" set NTHELP=none
goto opts
:optsdone
if "%NTDBG%"=="1" set NTARGS=%NTARGS% DEBUG=1
if "%NTDBG%"=="1" set NTOUT=Dbg_%NTCPU%
if "%NTSTAT%"=="1" set NTARGS=%NTARGS% STATICCRT=1
if "%NTVS%"=="1" set NTARGS=%NTARGS% VISUALSTYLES=1
if not "%NTCC%"=="" set NTARGS=%NTARGS% COMCTL=%NTCC%
if "%NTHELP%"=="" set NTHELP=mini
set NTARGS=%NTARGS% HELP=%NTHELP%

echo === Building for %NTCPU% into %NTOUT%
if "%NTSTAT%"=="1" echo === VNCHooks.dll uses the static CRT.
if "%NTHELP%"=="html" echo === HtmlHelp: htmlhelp.lib will be linked, it must be a %NTCPU% library.
if "%NTHELP%"=="none" echo === No context help.

echo === vncviewer\omnithread
cd vncviewer\omnithread
nmake /nologo /f nt.mak %NTARGS%
if errorlevel 1 goto fail2
echo === vncviewer\zlib
cd ..\zlib
nmake /nologo /f nt.mak %NTARGS%
if errorlevel 1 goto fail2
echo === vncviewer\libjpeg
cd ..\libjpeg
nmake /nologo /f nt.mak %NTARGS%
if errorlevel 1 goto fail2
echo === vncviewer
cd ..
nmake /nologo /f nt.mak %NTARGS%
if errorlevel 1 goto fail1

echo === winvnc\omnithread
cd ..\winvnc\omnithread
nmake /nologo /f nt.mak %NTARGS%
if errorlevel 1 goto fail2
echo === winvnc\zlib
cd ..\zlib
nmake /nologo /f nt.mak %NTARGS%
if errorlevel 1 goto fail2
echo === winvnc\libjpeg
cd ..\libjpeg
nmake /nologo /f nt.mak %NTARGS%
if errorlevel 1 goto fail2
echo === winvnc\VNCHooks
cd ..\VNCHooks
nmake /nologo /f nt.mak %NTARGS%
if errorlevel 1 goto fail2
echo === winvnc
cd ..
nmake /nologo /f nt.mak %NTARGS%
if errorlevel 1 goto fail1

rem WinVNC loads VNCHooks.dll from next to itself.
copy VNCHooks\%NTOUT%\VNCHooks.dll %NTOUT%\VNCHooks.dll >nul
cd ..
echo === Build succeeded.
goto end

:fail2
cd ..
:fail1
cd ..
echo === Build FAILED.

:end
set NTCPU=
set NTARGS=
set NTOUT=
set NTDBG=
set NTSTAT=
set NTVS=
set NTCC=
set NTHELP=
