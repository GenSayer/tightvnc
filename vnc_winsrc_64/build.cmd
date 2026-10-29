@echo off
rem build.cmd [X64|ARM64|ARM64EC|AXP64|IA64] [debug] [dllcrt] [visualstyles] [comctl4|5|6]
rem           [minihelp|htmlhelp|nohelp]
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
rem "dllcrt" builds VNCHooks.dll with the DLL CRT (/MD) instead of the static
rem CRT; the target then needs the compiler's CRT DLL (msvcr80.dll etc.).
rem "minihelp" (the default), "htmlhelp" or "nohelp" picks the context help
rem of the dialogs: the built-in text popups, the original HtmlHelp popups
rem (links htmlhelp.lib, which must match the CPU; warns for IA64 and AXP64),
rem or none (see nt.inc).
rem
rem "comctl4", "comctl5" or "comctl6" compiles for that Common Controls level
rem (_WIN32_IE; "comctl6" implies "visualstyles", see nt.inc).
rem "visualstyles" embeds a Common Controls 6 manifest, so the programs use the
rem current theme on Windows XP and later (see nt.inc).
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

rem Options after the CPU, in any order.
set NTDBG=
set NTVS=
set NTCC=
set NTHELP=
set NTDLL=
:opts
shift
if "%1"=="" goto optsdone
if "%1"=="debug" set NTDBG=1
if "%1"=="DEBUG" set NTDBG=1
if "%1"=="Debug" set NTDBG=1
if "%1"=="minihelp" set NTHELP=mini
if "%1"=="MINIHELP" set NTHELP=mini
if "%1"=="Minihelp" set NTHELP=mini
if "%1"=="htmlhelp" set NTHELP=html
if "%1"=="HTMLHELP" set NTHELP=html
if "%1"=="Htmlhelp" set NTHELP=html
if "%1"=="nohelp" set NTHELP=none
if "%1"=="NOHELP" set NTHELP=none
if "%1"=="Nohelp" set NTHELP=none
if "%1"=="dllcrt" set NTDLL=1
if "%1"=="DLLCRT" set NTDLL=1
if "%1"=="Dllcrt" set NTDLL=1
if "%1"=="visualstyles" set NTVS=1
if "%1"=="VISUALSTYLES" set NTVS=1
if "%1"=="Visualstyles" set NTVS=1
if "%1"=="comctl4" set NTCC=4
if "%1"=="comctl5" set NTCC=5
if "%1"=="comctl6" set NTCC=6
if "%1"=="COMCTL4" set NTCC=4
if "%1"=="COMCTL5" set NTCC=5
if "%1"=="COMCTL6" set NTCC=6
goto opts
:optsdone
if "%NTDBG%"=="1" set NTARGS=%NTARGS% DEBUG=1
if "%NTDBG%"=="1" set NTOUT=Dbg_%NTCPU%
if "%NTDLL%"=="1" set NTARGS=%NTARGS% DLLCRT=1
if "%NTVS%"=="1" set NTARGS=%NTARGS% VISUALSTYLES=1
if not "%NTCC%"=="" set NTARGS=%NTARGS% COMCTL=%NTCC%
if "%NTHELP%"=="" set NTHELP=mini
set NTARGS=%NTARGS% HELP=%NTHELP%

echo === Building for %NTCPU% into %NTOUT%
if "%NTDLL%"=="1" echo === VNCHooks.dll uses the DLL CRT: the target needs the compiler's CRT DLL installed.

if "%NTHELP%"=="none" echo === No context help.
if not "%NTHELP%"=="html" goto hhdone
echo === HtmlHelp: htmlhelp.lib will be linked, it must be a %NTCPU% library.
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
set NTHELP=
set NTDBG=
set NTDLL=
set NTVS=
set NTCC=
