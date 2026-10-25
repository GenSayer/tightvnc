@echo off
rem build.cmd [i386|ALPHA|ARM] [debug]
rem
rem Builds vncviewer and WinVNC (plus VNCHooks.dll) from the command line.
rem Run from the vnc_winsrc directory with the compiler environment for the
rem target CPU set up (PATH, INCLUDE, LIB): Visual C++ 4.x - 6.0 for i386 or
rem Alpha, a Visual Studio 2015 ARM environment (for example vcvarsall.bat
rem x86_arm) for ARM.  With no CPU argument, PROCESSOR_ARCHITECTURE picks it.
rem Output: <dir>\Rel_<CPU> or <dir>\Dbg_<CPU>.
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
if "%2"=="debug" goto dbg
if "%2"=="DEBUG" goto dbg
if "%2"=="Debug" goto dbg
goto nodbg
:dbg
set NTARGS=%NTARGS% DEBUG=1
set NTOUT=Dbg_%NTCPU%
:nodbg

echo === Building for %NTCPU% into %NTOUT%

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
