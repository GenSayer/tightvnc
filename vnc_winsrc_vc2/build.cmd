@echo off
rem build.cmd [i386|MIPS|ALPHA] [debug] [minihelp|nohelp]
rem
rem Builds vncviewer and WinVNC (plus VNCHooks.dll) with Visual C++ 2.x.
rem Run from the vnc_winsrc_vc2 directory with the VC++ 2.x environment set
rem (PATH, INCLUDE, LIB).  With no CPU argument, PROCESSOR_ARCHITECTURE picks
rem it.  Output: <dir>\Rel_<CPU> or <dir>\Dbg_<CPU>.
rem
rem "minihelp" (the default) or "nohelp" picks the context help of the
rem dialogs: the built-in text popups, or none (see vc2.inc).
rem
rem Written for the NT 3.1 command interpreter: no "call :label", no pushd.

set VC2ARGS=
set VC2CPU=%1
set VC2DBG=
set VC2HELP=
if not "%1"=="" set VC2ARGS=CPU=%1

rem Options after the CPU, in any order.
:opts
shift
if "%1"=="" goto optsdone
if "%1"=="debug" set VC2DBG=1
if "%1"=="DEBUG" set VC2DBG=1
if "%1"=="minihelp" set VC2HELP=mini
if "%1"=="MINIHELP" set VC2HELP=mini
if "%1"=="nohelp" set VC2HELP=none
if "%1"=="NOHELP" set VC2HELP=none
goto opts
:optsdone
set VC2OUT=Rel_%VC2CPU%
if "%VC2DBG%"=="1" set VC2ARGS=%VC2ARGS% DEBUG=1
if "%VC2DBG%"=="1" set VC2OUT=Dbg_%VC2CPU%
if "%VC2HELP%"=="" set VC2HELP=mini
set VC2ARGS=%VC2ARGS% HELP=%VC2HELP%
if "%VC2HELP%"=="none" echo === No context help.

echo === vc2ctl
cd vc2ctl
nmake /nologo /f vc2.mak %VC2ARGS%
if errorlevel 1 goto fail1
cd ..

echo === vncviewer\omnithread
cd vncviewer\omnithread
nmake /nologo /f vc2.mak %VC2ARGS%
if errorlevel 1 goto fail2
echo === vncviewer\zlib
cd ..\zlib
nmake /nologo /f vc2.mak %VC2ARGS%
if errorlevel 1 goto fail2
echo === vncviewer\libjpeg
cd ..\libjpeg
nmake /nologo /f vc2.mak %VC2ARGS%
if errorlevel 1 goto fail2
echo === vncviewer
cd ..
nmake /nologo /f vc2.mak %VC2ARGS%
if errorlevel 1 goto fail1

echo === winvnc\omnithread
cd ..\winvnc\omnithread
nmake /nologo /f vc2.mak %VC2ARGS%
if errorlevel 1 goto fail2
echo === winvnc\zlib
cd ..\zlib
nmake /nologo /f vc2.mak %VC2ARGS%
if errorlevel 1 goto fail2
echo === winvnc\libjpeg
cd ..\libjpeg
nmake /nologo /f vc2.mak %VC2ARGS%
if errorlevel 1 goto fail2
echo === winvnc\VNCHooks
cd ..\VNCHooks
nmake /nologo /f vc2.mak %VC2ARGS%
if errorlevel 1 goto fail2
echo === winvnc
cd ..
nmake /nologo /f vc2.mak %VC2ARGS%
if errorlevel 1 goto fail1

rem WinVNC looks for VNCHooks.dll next to itself.  Only copy when the CPU was
rem named, because the output directory name depends on it.
if "%1"=="" goto nocopy
copy VNCHooks\%VC2OUT%\VNCHooks.dll %VC2OUT%\VNCHooks.dll >nul
goto copied
:nocopy
echo Note: copy VNCHooks\Rel_CPU\VNCHooks.dll next to WinVNC.exe yourself.
:copied
cd ..
echo === Build succeeded.
goto end

:fail2
cd ..
:fail1
cd ..
echo === Build FAILED.

:end
set VC2ARGS=
set VC2OUT=
set VC2CPU=
set VC2DBG=
set VC2HELP=
