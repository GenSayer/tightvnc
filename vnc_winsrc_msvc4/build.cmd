@echo off
rem build.cmd [i386|MIPS|ALPHA|PPC] [debug] [minihelp|nohelp]
rem
rem Builds vncviewer and WinVNC (plus VNCHooks.dll) with Visual C++ 4.x.
rem Run from the vnc_winsrc_msvc4 directory with the VC++ 4.x environment set
rem (PATH, INCLUDE, LIB) for the target CPU.  With no CPU argument,
rem PROCESSOR_ARCHITECTURE picks it.  Output: <dir>\Rel_<CPU> or
rem <dir>\Dbg_<CPU>.
rem
rem "minihelp" (the default) or "nohelp" picks the context help of the
rem dialogs: the built-in text popups, or none (see msvc4.inc).
rem
rem The Developer Studio files (WinVNC.mak, vncviewer.mak, *.dsp and the
rem build.cmd in winvnc\ and vncviewer\) still work as before; this script
rem uses the msvc4.mak files instead.
rem
rem Written for the NT 3.5 command interpreter: no "call :label", no pushd,
rem no "if /i".

set M4CPU=%1
if "%M4CPU%"=="" set M4CPU=%PROCESSOR_ARCHITECTURE%
if "%M4CPU%"=="x86" set M4CPU=i386
if "%M4CPU%"=="X86" set M4CPU=i386
if "%M4CPU%"=="I386" set M4CPU=i386
if "%M4CPU%"=="mips" set M4CPU=MIPS
if "%M4CPU%"=="Mips" set M4CPU=MIPS
if "%M4CPU%"=="alpha" set M4CPU=ALPHA
if "%M4CPU%"=="Alpha" set M4CPU=ALPHA
if "%M4CPU%"=="ppc" set M4CPU=PPC
if "%M4CPU%"=="Ppc" set M4CPU=PPC
if "%M4CPU%"=="i386" goto cpuok
if "%M4CPU%"=="MIPS" goto cpuok
if "%M4CPU%"=="ALPHA" goto cpuok
if "%M4CPU%"=="PPC" goto cpuok
echo Unknown CPU "%M4CPU%".  Use i386, MIPS, ALPHA or PPC.
goto end
:cpuok

set M4ARGS=CPU=%M4CPU%
set M4OUT=Rel_%M4CPU%
set M4DBG=
set M4HELP=

rem Options after the CPU, in any order.
:opts
shift
if "%1"=="" goto optsdone
if "%1"=="debug" set M4DBG=1
if "%1"=="DEBUG" set M4DBG=1
if "%1"=="Debug" set M4DBG=1
if "%1"=="minihelp" set M4HELP=mini
if "%1"=="MINIHELP" set M4HELP=mini
if "%1"=="Minihelp" set M4HELP=mini
if "%1"=="nohelp" set M4HELP=none
if "%1"=="NOHELP" set M4HELP=none
if "%1"=="Nohelp" set M4HELP=none
goto opts
:optsdone
if "%M4DBG%"=="1" set M4ARGS=%M4ARGS% DEBUG=1
if "%M4DBG%"=="1" set M4OUT=Dbg_%M4CPU%
if "%M4HELP%"=="" set M4HELP=mini
set M4ARGS=%M4ARGS% HELP=%M4HELP%

echo === Building for %M4CPU% into %M4OUT%
if "%M4HELP%"=="none" echo === No context help.

echo === vc2ctl
cd vc2ctl
nmake /nologo /f msvc4.mak %M4ARGS%
if errorlevel 1 goto fail1
cd ..

echo === vncviewer\omnithread
cd vncviewer\omnithread
nmake /nologo /f msvc4.mak %M4ARGS%
if errorlevel 1 goto fail2
echo === vncviewer\zlib
cd ..\zlib
nmake /nologo /f msvc4.mak %M4ARGS%
if errorlevel 1 goto fail2
echo === vncviewer\libjpeg
cd ..\libjpeg
nmake /nologo /f msvc4.mak %M4ARGS%
if errorlevel 1 goto fail2
echo === vncviewer
cd ..
nmake /nologo /f msvc4.mak %M4ARGS%
if errorlevel 1 goto fail1

echo === winvnc\omnithread
cd ..\winvnc\omnithread
nmake /nologo /f msvc4.mak %M4ARGS%
if errorlevel 1 goto fail2
echo === winvnc\zlib
cd ..\zlib
nmake /nologo /f msvc4.mak %M4ARGS%
if errorlevel 1 goto fail2
echo === winvnc\libjpeg
cd ..\libjpeg
nmake /nologo /f msvc4.mak %M4ARGS%
if errorlevel 1 goto fail2
echo === winvnc\VNCHooks
cd ..\VNCHooks
nmake /nologo /f msvc4.mak %M4ARGS%
if errorlevel 1 goto fail2
echo === winvnc
cd ..
nmake /nologo /f msvc4.mak %M4ARGS%
if errorlevel 1 goto fail1

rem WinVNC loads VNCHooks.dll from next to itself.
copy VNCHooks\%M4OUT%\VNCHooks.dll %M4OUT%\VNCHooks.dll >nul
cd ..
echo === Build succeeded.
goto end

:fail2
cd ..
:fail1
cd ..
echo === Build FAILED.

:end
set M4CPU=
set M4ARGS=
set M4OUT=
set M4DBG=
set M4HELP=
