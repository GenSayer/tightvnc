@echo off
rem build.cmd [debug] [minihelp|nohelp]
rem
rem Builds vncviewer and WinVNC with Visual C++ for x86.
rem Run from the vnc_winsrc_win32s directory with the Visual C++ environment set
rem (PATH, INCLUDE, LIB).  Output: <dir>\Rel_i386 or <dir>\Dbg_i386.  Win32s
rem is x86-only; the RISC and PowerPC builds are in vnc_winsrc_msvc4.
rem
rem "minihelp" (the default) or "nohelp" picks the context help of the
rem dialogs: the built-in text popups, or none (see win32s.inc).
rem
rem Written for the NT 3.1 command interpreter: no "call :label", no pushd.

set W32ARGS=
set W32HELP=

rem Options, in any order.
:opts
if "%1"=="" goto optsdone
if "%1"=="debug" set W32ARGS=DEBUG=1
if "%1"=="DEBUG" set W32ARGS=DEBUG=1
if "%1"=="minihelp" set W32HELP=mini
if "%1"=="MINIHELP" set W32HELP=mini
if "%1"=="nohelp" set W32HELP=none
if "%1"=="NOHELP" set W32HELP=none
shift
goto opts
:optsdone
if "%W32HELP%"=="" set W32HELP=mini
set W32ARGS=%W32ARGS% HELP=%W32HELP%
if "%W32HELP%"=="none" echo === No context help.

echo === vncviewer\omnithread
cd vncviewer\omnithread
nmake /nologo /f win32s.mak %W32ARGS%
if errorlevel 1 goto fail2
echo === vncviewer\zlib
cd ..\zlib
nmake /nologo /f win32s.mak %W32ARGS%
if errorlevel 1 goto fail2
echo === vncviewer\libjpeg
cd ..\libjpeg
nmake /nologo /f win32s.mak %W32ARGS%
if errorlevel 1 goto fail2
echo === vncviewer
cd ..
nmake /nologo /f win32s.mak %W32ARGS%
if errorlevel 1 goto fail1

echo === winvnc\omnithread
cd ..\winvnc\omnithread
nmake /nologo /f win32s.mak %W32ARGS%
if errorlevel 1 goto fail2
echo === winvnc\zlib
cd ..\zlib
nmake /nologo /f win32s.mak %W32ARGS%
if errorlevel 1 goto fail2
echo === winvnc\libjpeg
cd ..\libjpeg
nmake /nologo /f win32s.mak %W32ARGS%
if errorlevel 1 goto fail2
echo === winvnc
cd ..
nmake /nologo /f win32s.mak %W32ARGS%
if errorlevel 1 goto fail1

cd ..
echo === Build succeeded.
goto end

:fail2
cd ..
:fail1
cd ..
echo === Build FAILED.

:end
set W32ARGS=
set W32HELP=
