@echo off
rem build.cmd [X64|ARM64|ARM64EC|AXP64|IA64] [debug] [htmlhelp]
rem
rem Builds vncviewer and WinVNC (plus VNCHooks.dll) from the command line.
rem Run from the vnc_winsrc_64 directory with the compiler environment for the
rem target CPU set up (PATH, INCLUDE, LIB): the Alpha64 or Itanium Platform
rem SDK compiler for AXP64 and IA64, a Visual C++ 2005 or later x64
rem environment for X64, a Visual Studio 2019/2022 ARM64 environment for ARM64
rem and ARM64EC.  With no CPU argument, PROCESSOR_ARCHITECTURE picks it (it
rem cannot pick ARM64EC, ask for that one by name).
rem Output: <dir>\Rel_<CPU> or <dir>\Dbg_<CPU>.
rem
rem The 32-bit CPUs i386, ALPHA and ARM are accepted too, but only for
rem testing; they are not supported in this tree.
rem
rem "htmlhelp" links htmlhelp.lib (the calls into it are commented out in the
rem sources; see nt.inc).  Warns for IA64 and AXP64.
rem
rem The Developer Studio files (WinVNC.mak, vncviewer.mak, *-axp64.mak,
rem *-ia64.mak, *.dsp, *.vcproj) still work as before; this script uses the
rem nt.mak files instead.
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
if "%NTCPU%"=="x64" set NTCPU=X64
if "%NTCPU%"=="amd64" set NTCPU=X64
if "%NTCPU%"=="Amd64" set NTCPU=X64
if "%NTCPU%"=="AMD64" set NTCPU=X64
if "%NTCPU%"=="arm64" set NTCPU=ARM64
if "%NTCPU%"=="Arm64" set NTCPU=ARM64
if "%NTCPU%"=="arm64ec" set NTCPU=ARM64EC
if "%NTCPU%"=="Arm64ec" set NTCPU=ARM64EC
if "%NTCPU%"=="Arm64EC" set NTCPU=ARM64EC
if "%NTCPU%"=="arm64EC" set NTCPU=ARM64EC
if "%NTCPU%"=="axp64" set NTCPU=AXP64
if "%NTCPU%"=="Axp64" set NTCPU=AXP64
if "%NTCPU%"=="alpha64" set NTCPU=AXP64
if "%NTCPU%"=="Alpha64" set NTCPU=AXP64
if "%NTCPU%"=="ALPHA64" set NTCPU=AXP64
if "%NTCPU%"=="ia64" set NTCPU=IA64
if "%NTCPU%"=="Ia64" set NTCPU=IA64
if "%NTCPU%"=="X64" goto cpuok
if "%NTCPU%"=="ARM64" goto cpuok
if "%NTCPU%"=="ARM64EC" goto cpuok
if "%NTCPU%"=="AXP64" goto cpuok
if "%NTCPU%"=="IA64" goto cpuok
if "%NTCPU%"=="i386" goto cpuok
if "%NTCPU%"=="ALPHA" goto cpuok
if "%NTCPU%"=="ARM" goto cpuok
echo Unknown CPU "%NTCPU%".  Use X64, ARM64, ARM64EC, AXP64 or IA64.
goto end
:cpuok

set NTARGS=CPU=%NTCPU%
set NTOUT=Rel_%NTCPU%
set NTHH=

rem Options: "debug" and "htmlhelp" in either order, in the second or third
rem position.
if "%2"=="debug" set NTARGS=%NTARGS% DEBUG=1
if "%2"=="DEBUG" set NTARGS=%NTARGS% DEBUG=1
if "%2"=="Debug" set NTARGS=%NTARGS% DEBUG=1
if "%3"=="debug" set NTARGS=%NTARGS% DEBUG=1
if "%3"=="DEBUG" set NTARGS=%NTARGS% DEBUG=1
if "%3"=="Debug" set NTARGS=%NTARGS% DEBUG=1
if "%2"=="htmlhelp" set NTHH=1
if "%2"=="HTMLHELP" set NTHH=1
if "%2"=="Htmlhelp" set NTHH=1
if "%3"=="htmlhelp" set NTHH=1
if "%3"=="HTMLHELP" set NTHH=1
if "%3"=="Htmlhelp" set NTHH=1
if "%2"=="debug" set NTOUT=Dbg_%NTCPU%
if "%2"=="DEBUG" set NTOUT=Dbg_%NTCPU%
if "%2"=="Debug" set NTOUT=Dbg_%NTCPU%
if "%3"=="debug" set NTOUT=Dbg_%NTCPU%
if "%3"=="DEBUG" set NTOUT=Dbg_%NTCPU%
if "%3"=="Debug" set NTOUT=Dbg_%NTCPU%
if "%NTHH%"=="1" set NTARGS=%NTARGS% HTMLHELP=1

echo === Building for %NTCPU% into %NTOUT%

if not "%NTHH%"=="1" goto hhdone
echo === htmlhelp.lib will be linked: it must be a %NTCPU% library.
if "%NTCPU%"=="IA64" echo === WARNING: HtmlHelp calls crash on IA64 before Windows Server 2003 SP1.
if "%NTCPU%"=="AXP64" echo === WARNING: AXP64 is an Alpha-only Windows build and may be unstable.
:hhdone

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
set NTHH=
