# Microsoft Developer Studio Generated NMAKE File, Based on WinVNC.dsp
!IF "$(CFG)" == ""
CFG=WinVNC - Win32 Profile
!MESSAGE No configuration specified. Defaulting to WinVNC - Win32 Profile.
!ENDIF 

!IF "$(CFG)" != "WinVNC - Win32 Release" && "$(CFG)" != "WinVNC - Win32 Debug" && "$(CFG)" != "WinVNC - Win32 Profile" && "$(CFG)" != "WinVNC - Win32 HorizonLive"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "WinVNC.mak" CFG="WinVNC - Win32 Profile"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "WinVNC - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "WinVNC - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE "WinVNC - Win32 Profile" (based on "Win32 (x86) Application")
!MESSAGE "WinVNC - Win32 HorizonLive" (based on "Win32 (x86) Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "WinVNC - Win32 Release"

OUTDIR=.\Release
INTDIR=.\./Release/WinVNC
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\WinVNC.exe"

!ELSE 

ALL : "libjpeg - Win32 Release" "zlib - Win32 Release" "omnithread - Win32 Release" "VNCHooks - Win32 Release" "$(OUTDIR)\WinVNC.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"VNCHooks - Win32 ReleaseCLEAN" "omnithread - Win32 ReleaseCLEAN" "zlib - Win32 ReleaseCLEAN" "libjpeg - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\AdministrationControls.obj"
	-@erase "$(INTDIR)\BuildTime.obj"
	-@erase "$(INTDIR)\d3des.obj"
	-@erase "$(INTDIR)\DynamicFn.obj"
	-@erase "$(INTDIR)\FileTransferItemInfo.obj"
	-@erase "$(INTDIR)\IncomingConnectionsControls.obj"
	-@erase "$(INTDIR)\InputHandlingControls.obj"
	-@erase "$(INTDIR)\Log.obj"
	-@erase "$(INTDIR)\MatchWindow.obj"
	-@erase "$(INTDIR)\MinMax.obj"
	-@erase "$(INTDIR)\ParseHost.obj"
	-@erase "$(INTDIR)\PollControls.obj"
	-@erase "$(INTDIR)\QuerySettingsControls.obj"
	-@erase "$(INTDIR)\RectList.obj"
	-@erase "$(INTDIR)\SharedDesktopArea.obj"
	-@erase "$(INTDIR)\stdhdrs.obj"
	-@erase "$(INTDIR)\translate.obj"
	-@erase "$(INTDIR)\TsSessions.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\VideoDriver.obj"
	-@erase "$(INTDIR)\vncAbout.obj"
	-@erase "$(INTDIR)\vncAcceptDialog.obj"
	-@erase "$(INTDIR)\vncAcceptReverseDlg.obj"
	-@erase "$(INTDIR)\vncauth.obj"
	-@erase "$(INTDIR)\vncBuffer.obj"
	-@erase "$(INTDIR)\vncClient.obj"
	-@erase "$(INTDIR)\vncConnDialog.obj"
	-@erase "$(INTDIR)\vncDesktop.obj"
	-@erase "$(INTDIR)\vncEncodeCoRRE.obj"
	-@erase "$(INTDIR)\vncEncodeHexT.obj"
	-@erase "$(INTDIR)\vncEncoder.obj"
	-@erase "$(INTDIR)\vncEncodeRRE.obj"
	-@erase "$(INTDIR)\vncEncodeTight.obj"
	-@erase "$(INTDIR)\vncEncodeZlib.obj"
	-@erase "$(INTDIR)\vncEncodeZlibHex.obj"
	-@erase "$(INTDIR)\VNCHelp.obj"
	-@erase "$(INTDIR)\vncHTTPConnect.obj"
	-@erase "$(INTDIR)\vncInstHandler.obj"
	-@erase "$(INTDIR)\vncKeymap.obj"
	-@erase "$(INTDIR)\vncMenu.obj"
	-@erase "$(INTDIR)\vncProperties.obj"
	-@erase "$(INTDIR)\vncRegion.obj"
	-@erase "$(INTDIR)\vncServer.obj"
	-@erase "$(INTDIR)\vncService.obj"
	-@erase "$(INTDIR)\vncSockConnect.obj"
	-@erase "$(INTDIR)\vncTimedMsgBox.obj"
	-@erase "$(INTDIR)\VSocket.obj"
	-@erase "$(INTDIR)\WallpaperUtils.obj"
	-@erase "$(INTDIR)\WinVNC.obj"
	-@erase "$(INTDIR)\WinVNC.res"
	-@erase "$(OUTDIR)\WinVNC.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "./omnithread" /I "./zlib" /I ".." /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "__WIN32__" /D "__NT__" /D "__x86__" /D "_WINSTATIC" /D "NCORBA" /D WINVER=0x0500 /Fp"$(INTDIR)\WinVNC.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32 
RSC=rc.exe
RSC_PROJ=/l 0x809 /fo"$(INTDIR)\WinVNC.res" /d "NDEBUG" /d "WITH_JAVA_VIEWER" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\WinVNC.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comctl32.lib htmlhelp.lib /nologo /subsystem:windows /incremental:no /pdb:"$(OUTDIR)\WinVNC.pdb" /machine:IA64 /out:"$(OUTDIR)\WinVNC.exe" 
LINK32_OBJS= \
	"$(INTDIR)\AdministrationControls.obj" \
	"$(INTDIR)\BuildTime.obj" \
	"$(INTDIR)\d3des.obj" \
	"$(INTDIR)\DynamicFn.obj" \
	"$(INTDIR)\FileTransferItemInfo.obj" \
	"$(INTDIR)\IncomingConnectionsControls.obj" \
	"$(INTDIR)\InputHandlingControls.obj" \
	"$(INTDIR)\Log.obj" \
	"$(INTDIR)\MatchWindow.obj" \
	"$(INTDIR)\MinMax.obj" \
	"$(INTDIR)\ParseHost.obj" \
	"$(INTDIR)\PollControls.obj" \
	"$(INTDIR)\QuerySettingsControls.obj" \
	"$(INTDIR)\RectList.obj" \
	"$(INTDIR)\SharedDesktopArea.obj" \
	"$(INTDIR)\stdhdrs.obj" \
	"$(INTDIR)\translate.obj" \
	"$(INTDIR)\TsSessions.obj" \
	"$(INTDIR)\VideoDriver.obj" \
	"$(INTDIR)\vncAbout.obj" \
	"$(INTDIR)\vncAcceptDialog.obj" \
	"$(INTDIR)\vncAcceptReverseDlg.obj" \
	"$(INTDIR)\vncauth.obj" \
	"$(INTDIR)\vncBuffer.obj" \
	"$(INTDIR)\vncClient.obj" \
	"$(INTDIR)\vncConnDialog.obj" \
	"$(INTDIR)\vncDesktop.obj" \
	"$(INTDIR)\vncEncodeCoRRE.obj" \
	"$(INTDIR)\vncEncodeHexT.obj" \
	"$(INTDIR)\vncEncoder.obj" \
	"$(INTDIR)\vncEncodeRRE.obj" \
	"$(INTDIR)\vncEncodeTight.obj" \
	"$(INTDIR)\vncEncodeZlib.obj" \
	"$(INTDIR)\vncEncodeZlibHex.obj" \
	"$(INTDIR)\VNCHelp.obj" \
	"$(INTDIR)\vncHTTPConnect.obj" \
	"$(INTDIR)\vncInstHandler.obj" \
	"$(INTDIR)\vncKeymap.obj" \
	"$(INTDIR)\vncMenu.obj" \
	"$(INTDIR)\vncProperties.obj" \
	"$(INTDIR)\vncRegion.obj" \
	"$(INTDIR)\vncServer.obj" \
	"$(INTDIR)\vncService.obj" \
	"$(INTDIR)\vncSockConnect.obj" \
	"$(INTDIR)\vncTimedMsgBox.obj" \
	"$(INTDIR)\VSocket.obj" \
	"$(INTDIR)\WallpaperUtils.obj" \
	"$(INTDIR)\WinVNC.obj" \
	"$(INTDIR)\WinVNC.res" \
	"$(OUTDIR)\VNCHooks.lib" \
	"$(OUTDIR)\omnithread.lib" \
	"$(OUTDIR)\zlib.lib" \
	"$(OUTDIR)\libjpeg.lib"

"$(OUTDIR)\WinVNC.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
   cl /c /nologo /Fo.\Release\winvnc\ /Fd.\Release\winvnc /MT BuildTime.cpp
	 $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"

!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\./Debug/WinVNC
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\WinVNC.exe" "$(OUTDIR)\WinVNC.bsc"

!ELSE 

ALL : "libjpeg - Win32 Debug" "zlib - Win32 Debug" "omnithread - Win32 Debug" "VNCHooks - Win32 Debug" "$(OUTDIR)\WinVNC.exe" "$(OUTDIR)\WinVNC.bsc"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"VNCHooks - Win32 DebugCLEAN" "omnithread - Win32 DebugCLEAN" "zlib - Win32 DebugCLEAN" "libjpeg - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\AdministrationControls.obj"
	-@erase "$(INTDIR)\AdministrationControls.sbr"
	-@erase "$(INTDIR)\BuildTime.obj"
	-@erase "$(INTDIR)\BuildTime.sbr"
	-@erase "$(INTDIR)\d3des.obj"
	-@erase "$(INTDIR)\d3des.sbr"
	-@erase "$(INTDIR)\DynamicFn.obj"
	-@erase "$(INTDIR)\DynamicFn.sbr"
	-@erase "$(INTDIR)\FileTransferItemInfo.obj"
	-@erase "$(INTDIR)\FileTransferItemInfo.sbr"
	-@erase "$(INTDIR)\IncomingConnectionsControls.obj"
	-@erase "$(INTDIR)\IncomingConnectionsControls.sbr"
	-@erase "$(INTDIR)\InputHandlingControls.obj"
	-@erase "$(INTDIR)\InputHandlingControls.sbr"
	-@erase "$(INTDIR)\Log.obj"
	-@erase "$(INTDIR)\Log.sbr"
	-@erase "$(INTDIR)\MatchWindow.obj"
	-@erase "$(INTDIR)\MatchWindow.sbr"
	-@erase "$(INTDIR)\MinMax.obj"
	-@erase "$(INTDIR)\MinMax.sbr"
	-@erase "$(INTDIR)\ParseHost.obj"
	-@erase "$(INTDIR)\ParseHost.sbr"
	-@erase "$(INTDIR)\PollControls.obj"
	-@erase "$(INTDIR)\PollControls.sbr"
	-@erase "$(INTDIR)\QuerySettingsControls.obj"
	-@erase "$(INTDIR)\QuerySettingsControls.sbr"
	-@erase "$(INTDIR)\RectList.obj"
	-@erase "$(INTDIR)\RectList.sbr"
	-@erase "$(INTDIR)\SharedDesktopArea.obj"
	-@erase "$(INTDIR)\SharedDesktopArea.sbr"
	-@erase "$(INTDIR)\stdhdrs.obj"
	-@erase "$(INTDIR)\stdhdrs.sbr"
	-@erase "$(INTDIR)\translate.obj"
	-@erase "$(INTDIR)\translate.sbr"
	-@erase "$(INTDIR)\TsSessions.obj"
	-@erase "$(INTDIR)\TsSessions.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\VideoDriver.obj"
	-@erase "$(INTDIR)\VideoDriver.sbr"
	-@erase "$(INTDIR)\vncAbout.obj"
	-@erase "$(INTDIR)\vncAbout.sbr"
	-@erase "$(INTDIR)\vncAcceptDialog.obj"
	-@erase "$(INTDIR)\vncAcceptDialog.sbr"
	-@erase "$(INTDIR)\vncAcceptReverseDlg.obj"
	-@erase "$(INTDIR)\vncAcceptReverseDlg.sbr"
	-@erase "$(INTDIR)\vncauth.obj"
	-@erase "$(INTDIR)\vncauth.sbr"
	-@erase "$(INTDIR)\vncBuffer.obj"
	-@erase "$(INTDIR)\vncBuffer.sbr"
	-@erase "$(INTDIR)\vncClient.obj"
	-@erase "$(INTDIR)\vncClient.sbr"
	-@erase "$(INTDIR)\vncConnDialog.obj"
	-@erase "$(INTDIR)\vncConnDialog.sbr"
	-@erase "$(INTDIR)\vncDesktop.obj"
	-@erase "$(INTDIR)\vncDesktop.sbr"
	-@erase "$(INTDIR)\vncEncodeCoRRE.obj"
	-@erase "$(INTDIR)\vncEncodeCoRRE.sbr"
	-@erase "$(INTDIR)\vncEncodeHexT.obj"
	-@erase "$(INTDIR)\vncEncodeHexT.sbr"
	-@erase "$(INTDIR)\vncEncoder.obj"
	-@erase "$(INTDIR)\vncEncoder.sbr"
	-@erase "$(INTDIR)\vncEncodeRRE.obj"
	-@erase "$(INTDIR)\vncEncodeRRE.sbr"
	-@erase "$(INTDIR)\vncEncodeTight.obj"
	-@erase "$(INTDIR)\vncEncodeTight.sbr"
	-@erase "$(INTDIR)\vncEncodeZlib.obj"
	-@erase "$(INTDIR)\vncEncodeZlib.sbr"
	-@erase "$(INTDIR)\vncEncodeZlibHex.obj"
	-@erase "$(INTDIR)\vncEncodeZlibHex.sbr"
	-@erase "$(INTDIR)\VNCHelp.obj"
	-@erase "$(INTDIR)\VNCHelp.sbr"
	-@erase "$(INTDIR)\vncHTTPConnect.obj"
	-@erase "$(INTDIR)\vncHTTPConnect.sbr"
	-@erase "$(INTDIR)\vncInstHandler.obj"
	-@erase "$(INTDIR)\vncInstHandler.sbr"
	-@erase "$(INTDIR)\vncKeymap.obj"
	-@erase "$(INTDIR)\vncKeymap.sbr"
	-@erase "$(INTDIR)\vncMenu.obj"
	-@erase "$(INTDIR)\vncMenu.sbr"
	-@erase "$(INTDIR)\vncProperties.obj"
	-@erase "$(INTDIR)\vncProperties.sbr"
	-@erase "$(INTDIR)\vncRegion.obj"
	-@erase "$(INTDIR)\vncRegion.sbr"
	-@erase "$(INTDIR)\vncServer.obj"
	-@erase "$(INTDIR)\vncServer.sbr"
	-@erase "$(INTDIR)\vncService.obj"
	-@erase "$(INTDIR)\vncService.sbr"
	-@erase "$(INTDIR)\vncSockConnect.obj"
	-@erase "$(INTDIR)\vncSockConnect.sbr"
	-@erase "$(INTDIR)\vncTimedMsgBox.obj"
	-@erase "$(INTDIR)\vncTimedMsgBox.sbr"
	-@erase "$(INTDIR)\VSocket.obj"
	-@erase "$(INTDIR)\VSocket.sbr"
	-@erase "$(INTDIR)\WallpaperUtils.obj"
	-@erase "$(INTDIR)\WallpaperUtils.sbr"
	-@erase "$(INTDIR)\WinVNC.obj"
	-@erase "$(INTDIR)\WinVNC.res"
	-@erase "$(INTDIR)\WinVNC.sbr"
	-@erase "$(OUTDIR)\WinVNC.bsc"
	-@erase "$(OUTDIR)\WinVNC.exe"
	-@erase "$(OUTDIR)\WinVNC.ilk"
	-@erase "$(OUTDIR)\WinVNC.pdb"
	-@erase "$(OUTDIR)\WinVNC\WinVNC.map"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od /I "./omnithread" /I "./zlib" /I ".." /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "__WIN32__" /D "__NT__" /D "__x86__" /D "_WINSTATIC" /D "NCORBA" /D WINVER=0x0500 /FR"$(INTDIR)\\" /Fp"$(INTDIR)\WinVNC.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32 
RSC=rc.exe
RSC_PROJ=/l 0x809 /fo"$(INTDIR)\WinVNC.res" /d "_DEBUG" /d "WITH_JAVA_VIEWER" 
BSC32_SBRS= \
	"$(INTDIR)\AdministrationControls.sbr" \
	"$(INTDIR)\BuildTime.sbr" \
	"$(INTDIR)\d3des.sbr" \
	"$(INTDIR)\DynamicFn.sbr" \
	"$(INTDIR)\FileTransferItemInfo.sbr" \
	"$(INTDIR)\IncomingConnectionsControls.sbr" \
	"$(INTDIR)\InputHandlingControls.sbr" \
	"$(INTDIR)\Log.sbr" \
	"$(INTDIR)\MatchWindow.sbr" \
	"$(INTDIR)\MinMax.sbr" \
	"$(INTDIR)\ParseHost.sbr" \
	"$(INTDIR)\PollControls.sbr" \
	"$(INTDIR)\QuerySettingsControls.sbr" \
	"$(INTDIR)\RectList.sbr" \
	"$(INTDIR)\SharedDesktopArea.sbr" \
	"$(INTDIR)\stdhdrs.sbr" \
	"$(INTDIR)\translate.sbr" \
	"$(INTDIR)\TsSessions.sbr" \
	"$(INTDIR)\VideoDriver.sbr" \
	"$(INTDIR)\vncAbout.sbr" \
	"$(INTDIR)\vncAcceptDialog.sbr" \
	"$(INTDIR)\vncAcceptReverseDlg.sbr" \
	"$(INTDIR)\vncauth.sbr" \
	"$(INTDIR)\vncBuffer.sbr" \
	"$(INTDIR)\vncClient.sbr" \
	"$(INTDIR)\vncConnDialog.sbr" \
	"$(INTDIR)\vncDesktop.sbr" \
	"$(INTDIR)\vncEncodeCoRRE.sbr" \
	"$(INTDIR)\vncEncodeHexT.sbr" \
	"$(INTDIR)\vncEncoder.sbr" \
	"$(INTDIR)\vncEncodeRRE.sbr" \
	"$(INTDIR)\vncEncodeTight.sbr" \
	"$(INTDIR)\vncEncodeZlib.sbr" \
	"$(INTDIR)\vncEncodeZlibHex.sbr" \
	"$(INTDIR)\VNCHelp.sbr" \
	"$(INTDIR)\vncHTTPConnect.sbr" \
	"$(INTDIR)\vncInstHandler.sbr" \
	"$(INTDIR)\vncKeymap.sbr" \
	"$(INTDIR)\vncMenu.sbr" \
	"$(INTDIR)\vncProperties.sbr" \
	"$(INTDIR)\vncRegion.sbr" \
	"$(INTDIR)\vncServer.sbr" \
	"$(INTDIR)\vncService.sbr" \
	"$(INTDIR)\vncSockConnect.sbr" \
	"$(INTDIR)\vncTimedMsgBox.sbr" \
	"$(INTDIR)\VSocket.sbr" \
	"$(INTDIR)\WallpaperUtils.sbr" \
	"$(INTDIR)\WinVNC.sbr"

"$(OUTDIR)\WinVNC.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comctl32.lib htmlhelp.lib /nologo /subsystem:windows /incremental:yes /pdb:"$(OUTDIR)\WinVNC.pdb" /map:"$(INTDIR)\WinVNC.map" /debug /machine:IA64 /out:"$(OUTDIR)\WinVNC.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\AdministrationControls.obj" \
	"$(INTDIR)\BuildTime.obj" \
	"$(INTDIR)\d3des.obj" \
	"$(INTDIR)\DynamicFn.obj" \
	"$(INTDIR)\FileTransferItemInfo.obj" \
	"$(INTDIR)\IncomingConnectionsControls.obj" \
	"$(INTDIR)\InputHandlingControls.obj" \
	"$(INTDIR)\Log.obj" \
	"$(INTDIR)\MatchWindow.obj" \
	"$(INTDIR)\MinMax.obj" \
	"$(INTDIR)\ParseHost.obj" \
	"$(INTDIR)\PollControls.obj" \
	"$(INTDIR)\QuerySettingsControls.obj" \
	"$(INTDIR)\RectList.obj" \
	"$(INTDIR)\SharedDesktopArea.obj" \
	"$(INTDIR)\stdhdrs.obj" \
	"$(INTDIR)\translate.obj" \
	"$(INTDIR)\TsSessions.obj" \
	"$(INTDIR)\VideoDriver.obj" \
	"$(INTDIR)\vncAbout.obj" \
	"$(INTDIR)\vncAcceptDialog.obj" \
	"$(INTDIR)\vncAcceptReverseDlg.obj" \
	"$(INTDIR)\vncauth.obj" \
	"$(INTDIR)\vncBuffer.obj" \
	"$(INTDIR)\vncClient.obj" \
	"$(INTDIR)\vncConnDialog.obj" \
	"$(INTDIR)\vncDesktop.obj" \
	"$(INTDIR)\vncEncodeCoRRE.obj" \
	"$(INTDIR)\vncEncodeHexT.obj" \
	"$(INTDIR)\vncEncoder.obj" \
	"$(INTDIR)\vncEncodeRRE.obj" \
	"$(INTDIR)\vncEncodeTight.obj" \
	"$(INTDIR)\vncEncodeZlib.obj" \
	"$(INTDIR)\vncEncodeZlibHex.obj" \
	"$(INTDIR)\VNCHelp.obj" \
	"$(INTDIR)\vncHTTPConnect.obj" \
	"$(INTDIR)\vncInstHandler.obj" \
	"$(INTDIR)\vncKeymap.obj" \
	"$(INTDIR)\vncMenu.obj" \
	"$(INTDIR)\vncProperties.obj" \
	"$(INTDIR)\vncRegion.obj" \
	"$(INTDIR)\vncServer.obj" \
	"$(INTDIR)\vncService.obj" \
	"$(INTDIR)\vncSockConnect.obj" \
	"$(INTDIR)\vncTimedMsgBox.obj" \
	"$(INTDIR)\VSocket.obj" \
	"$(INTDIR)\WallpaperUtils.obj" \
	"$(INTDIR)\WinVNC.obj" \
	"$(INTDIR)\WinVNC.res" \
	"$(OUTDIR)\VNCHooks.lib" \
	"$(OUTDIR)\omnithread.lib" \
	"$(OUTDIR)\zlib.lib" \
	"$(OUTDIR)\libjpeg.lib"

"$(OUTDIR)\WinVNC.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
   cl /c /nologo /Fo.\Debug\winvnc\ /Fd.\Debug\winvnc /MTd BuildTime.cpp
	 $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"

!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"

OUTDIR=.\Profile
INTDIR=.\./Profile/WinVNC
# Begin Custom Macros
OutDir=.\Profile
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\WinVNC.exe"

!ELSE 

ALL : "libjpeg - Win32 Profile" "zlib - Win32 Profile" "omnithread - Win32 Profile" "VNCHooks - Win32 Profile" "$(OUTDIR)\WinVNC.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"VNCHooks - Win32 ProfileCLEAN" "omnithread - Win32 ProfileCLEAN" "zlib - Win32 ProfileCLEAN" "libjpeg - Win32 ProfileCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\AdministrationControls.obj"
	-@erase "$(INTDIR)\BuildTime.obj"
	-@erase "$(INTDIR)\d3des.obj"
	-@erase "$(INTDIR)\DynamicFn.obj"
	-@erase "$(INTDIR)\FileTransferItemInfo.obj"
	-@erase "$(INTDIR)\IncomingConnectionsControls.obj"
	-@erase "$(INTDIR)\InputHandlingControls.obj"
	-@erase "$(INTDIR)\Log.obj"
	-@erase "$(INTDIR)\MatchWindow.obj"
	-@erase "$(INTDIR)\MinMax.obj"
	-@erase "$(INTDIR)\ParseHost.obj"
	-@erase "$(INTDIR)\PollControls.obj"
	-@erase "$(INTDIR)\QuerySettingsControls.obj"
	-@erase "$(INTDIR)\RectList.obj"
	-@erase "$(INTDIR)\SharedDesktopArea.obj"
	-@erase "$(INTDIR)\stdhdrs.obj"
	-@erase "$(INTDIR)\translate.obj"
	-@erase "$(INTDIR)\TsSessions.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\VideoDriver.obj"
	-@erase "$(INTDIR)\vncAbout.obj"
	-@erase "$(INTDIR)\vncAcceptDialog.obj"
	-@erase "$(INTDIR)\vncAcceptReverseDlg.obj"
	-@erase "$(INTDIR)\vncauth.obj"
	-@erase "$(INTDIR)\vncBuffer.obj"
	-@erase "$(INTDIR)\vncClient.obj"
	-@erase "$(INTDIR)\vncConnDialog.obj"
	-@erase "$(INTDIR)\vncDesktop.obj"
	-@erase "$(INTDIR)\vncEncodeCoRRE.obj"
	-@erase "$(INTDIR)\vncEncodeHexT.obj"
	-@erase "$(INTDIR)\vncEncoder.obj"
	-@erase "$(INTDIR)\vncEncodeRRE.obj"
	-@erase "$(INTDIR)\vncEncodeTight.obj"
	-@erase "$(INTDIR)\vncEncodeZlib.obj"
	-@erase "$(INTDIR)\vncEncodeZlibHex.obj"
	-@erase "$(INTDIR)\VNCHelp.obj"
	-@erase "$(INTDIR)\vncHTTPConnect.obj"
	-@erase "$(INTDIR)\vncInstHandler.obj"
	-@erase "$(INTDIR)\vncKeymap.obj"
	-@erase "$(INTDIR)\vncMenu.obj"
	-@erase "$(INTDIR)\vncProperties.obj"
	-@erase "$(INTDIR)\vncRegion.obj"
	-@erase "$(INTDIR)\vncServer.obj"
	-@erase "$(INTDIR)\vncService.obj"
	-@erase "$(INTDIR)\vncSockConnect.obj"
	-@erase "$(INTDIR)\vncTimedMsgBox.obj"
	-@erase "$(INTDIR)\VSocket.obj"
	-@erase "$(INTDIR)\WallpaperUtils.obj"
	-@erase "$(INTDIR)\WinVNC.obj"
	-@erase "$(INTDIR)\WinVNC.res"
	-@erase "$(OUTDIR)\WinVNC.exe"
	-@erase "$(OUTDIR)\WinVNC\WinVNC.map"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od /I "./omnithread" /I "./zlib" /I ".." /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "__WIN32__" /D "__NT__" /D "__x86__" /D "_WINSTATIC" /D "NCORBA" /D WINVER=0x0500 /Fp"$(INTDIR)\WinVNC.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32 
RSC=rc.exe
RSC_PROJ=/l 0x809 /fo"$(INTDIR)\WinVNC.res" /d "_DEBUG" /d "WITH_JAVA_VIEWER" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\WinVNC.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comctl32.lib htmlhelp.lib /nologo /subsystem:windows /profile /map:"$(INTDIR)\WinVNC.map" /debug /machine:IA64 /out:"$(OUTDIR)\WinVNC.exe" 
LINK32_OBJS= \
	"$(INTDIR)\AdministrationControls.obj" \
	"$(INTDIR)\BuildTime.obj" \
	"$(INTDIR)\d3des.obj" \
	"$(INTDIR)\DynamicFn.obj" \
	"$(INTDIR)\FileTransferItemInfo.obj" \
	"$(INTDIR)\IncomingConnectionsControls.obj" \
	"$(INTDIR)\InputHandlingControls.obj" \
	"$(INTDIR)\Log.obj" \
	"$(INTDIR)\MatchWindow.obj" \
	"$(INTDIR)\MinMax.obj" \
	"$(INTDIR)\ParseHost.obj" \
	"$(INTDIR)\PollControls.obj" \
	"$(INTDIR)\QuerySettingsControls.obj" \
	"$(INTDIR)\RectList.obj" \
	"$(INTDIR)\SharedDesktopArea.obj" \
	"$(INTDIR)\stdhdrs.obj" \
	"$(INTDIR)\translate.obj" \
	"$(INTDIR)\TsSessions.obj" \
	"$(INTDIR)\VideoDriver.obj" \
	"$(INTDIR)\vncAbout.obj" \
	"$(INTDIR)\vncAcceptDialog.obj" \
	"$(INTDIR)\vncAcceptReverseDlg.obj" \
	"$(INTDIR)\vncauth.obj" \
	"$(INTDIR)\vncBuffer.obj" \
	"$(INTDIR)\vncClient.obj" \
	"$(INTDIR)\vncConnDialog.obj" \
	"$(INTDIR)\vncDesktop.obj" \
	"$(INTDIR)\vncEncodeCoRRE.obj" \
	"$(INTDIR)\vncEncodeHexT.obj" \
	"$(INTDIR)\vncEncoder.obj" \
	"$(INTDIR)\vncEncodeRRE.obj" \
	"$(INTDIR)\vncEncodeTight.obj" \
	"$(INTDIR)\vncEncodeZlib.obj" \
	"$(INTDIR)\vncEncodeZlibHex.obj" \
	"$(INTDIR)\VNCHelp.obj" \
	"$(INTDIR)\vncHTTPConnect.obj" \
	"$(INTDIR)\vncInstHandler.obj" \
	"$(INTDIR)\vncKeymap.obj" \
	"$(INTDIR)\vncMenu.obj" \
	"$(INTDIR)\vncProperties.obj" \
	"$(INTDIR)\vncRegion.obj" \
	"$(INTDIR)\vncServer.obj" \
	"$(INTDIR)\vncService.obj" \
	"$(INTDIR)\vncSockConnect.obj" \
	"$(INTDIR)\vncTimedMsgBox.obj" \
	"$(INTDIR)\VSocket.obj" \
	"$(INTDIR)\WallpaperUtils.obj" \
	"$(INTDIR)\WinVNC.obj" \
	"$(INTDIR)\WinVNC.res" \
	"$(OUTDIR)\VNCHooks.lib" \
	"$(OUTDIR)\omnithread.lib" \
	"$(OUTDIR)\zlib.lib" \
	"$(OUTDIR)\libjpeg.lib"

"$(OUTDIR)\WinVNC.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
   cl /c /nologo /Fo.\Profile\winvnc\ /Fd.\Profile\winvnc /MT BuildTime.cpp
	 $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"

!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"

OUTDIR=.\HorizonLive
INTDIR=.\HorizonLive
# Begin Custom Macros
OutDir=.\HorizonLive
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\AppShare.exe"

!ELSE 

ALL : "libjpeg - Win32 HorizonLive" "zlib - Win32 HorizonLive" "omnithread - Win32 HorizonLive" "VNCHooks - Win32 HorizonLive" "$(OUTDIR)\AppShare.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"VNCHooks - Win32 HorizonLiveCLEAN" "omnithread - Win32 HorizonLiveCLEAN" "zlib - Win32 HorizonLiveCLEAN" "libjpeg - Win32 HorizonLiveCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\AdministrationControls.obj"
	-@erase "$(INTDIR)\BuildTime.obj"
	-@erase "$(INTDIR)\d3des.obj"
	-@erase "$(INTDIR)\DynamicFn.obj"
	-@erase "$(INTDIR)\FileTransferItemInfo.obj"
	-@erase "$(INTDIR)\IncomingConnectionsControls.obj"
	-@erase "$(INTDIR)\InputHandlingControls.obj"
	-@erase "$(INTDIR)\Log.obj"
	-@erase "$(INTDIR)\MatchWindow.obj"
	-@erase "$(INTDIR)\MinMax.obj"
	-@erase "$(INTDIR)\ParseHost.obj"
	-@erase "$(INTDIR)\PollControls.obj"
	-@erase "$(INTDIR)\QuerySettingsControls.obj"
	-@erase "$(INTDIR)\RectList.obj"
	-@erase "$(INTDIR)\SharedDesktopArea.obj"
	-@erase "$(INTDIR)\stdhdrs.obj"
	-@erase "$(INTDIR)\translate.obj"
	-@erase "$(INTDIR)\TsSessions.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\VideoDriver.obj"
	-@erase "$(INTDIR)\vncAbout.obj"
	-@erase "$(INTDIR)\vncAcceptDialog.obj"
	-@erase "$(INTDIR)\vncAcceptReverseDlg.obj"
	-@erase "$(INTDIR)\vncauth.obj"
	-@erase "$(INTDIR)\vncBuffer.obj"
	-@erase "$(INTDIR)\vncClient.obj"
	-@erase "$(INTDIR)\vncConnDialog.obj"
	-@erase "$(INTDIR)\vncDesktop.obj"
	-@erase "$(INTDIR)\vncEncodeCoRRE.obj"
	-@erase "$(INTDIR)\vncEncodeHexT.obj"
	-@erase "$(INTDIR)\vncEncoder.obj"
	-@erase "$(INTDIR)\vncEncodeRRE.obj"
	-@erase "$(INTDIR)\vncEncodeTight.obj"
	-@erase "$(INTDIR)\vncEncodeZlib.obj"
	-@erase "$(INTDIR)\vncEncodeZlibHex.obj"
	-@erase "$(INTDIR)\VNCHelp.obj"
	-@erase "$(INTDIR)\vncHTTPConnect.obj"
	-@erase "$(INTDIR)\vncInstHandler.obj"
	-@erase "$(INTDIR)\vncKeymap.obj"
	-@erase "$(INTDIR)\vncMenu.obj"
	-@erase "$(INTDIR)\vncProperties.obj"
	-@erase "$(INTDIR)\vncRegion.obj"
	-@erase "$(INTDIR)\vncServer.obj"
	-@erase "$(INTDIR)\vncService.obj"
	-@erase "$(INTDIR)\vncSockConnect.obj"
	-@erase "$(INTDIR)\vncTimedMsgBox.obj"
	-@erase "$(INTDIR)\VSocket.obj"
	-@erase "$(INTDIR)\WallpaperUtils.obj"
	-@erase "$(INTDIR)\WinVNC.obj"
	-@erase "$(INTDIR)\WinVNC.res"
	-@erase "$(OUTDIR)\AppShare.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "./omnithread" /I "./zlib" /I ".." /D "NDEBUG" /D "XMD_H" /D "HORIZONLIVE" /D "WIN32" /D "_WINDOWS" /D "__WIN32__" /D "__NT__" /D "__x86__" /D "_WINSTATIC" /D "NCORBA" /D WINVER=0x0500 /Fp"$(INTDIR)\WinVNC.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32 
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\WinVNC.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\WinVNC.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comctl32.lib htmlhelp.lib /nologo /subsystem:windows /incremental:no /pdb:"$(OUTDIR)\AppShare.pdb" /machine:IA64 /nodefaultlib:"LIBC" /out:"$(OUTDIR)\AppShare.exe" 
LINK32_OBJS= \
	"$(INTDIR)\AdministrationControls.obj" \
	"$(INTDIR)\BuildTime.obj" \
	"$(INTDIR)\d3des.obj" \
	"$(INTDIR)\DynamicFn.obj" \
	"$(INTDIR)\FileTransferItemInfo.obj" \
	"$(INTDIR)\IncomingConnectionsControls.obj" \
	"$(INTDIR)\InputHandlingControls.obj" \
	"$(INTDIR)\Log.obj" \
	"$(INTDIR)\MatchWindow.obj" \
	"$(INTDIR)\MinMax.obj" \
	"$(INTDIR)\ParseHost.obj" \
	"$(INTDIR)\PollControls.obj" \
	"$(INTDIR)\QuerySettingsControls.obj" \
	"$(INTDIR)\RectList.obj" \
	"$(INTDIR)\SharedDesktopArea.obj" \
	"$(INTDIR)\stdhdrs.obj" \
	"$(INTDIR)\translate.obj" \
	"$(INTDIR)\TsSessions.obj" \
	"$(INTDIR)\VideoDriver.obj" \
	"$(INTDIR)\vncAbout.obj" \
	"$(INTDIR)\vncAcceptDialog.obj" \
	"$(INTDIR)\vncAcceptReverseDlg.obj" \
	"$(INTDIR)\vncauth.obj" \
	"$(INTDIR)\vncBuffer.obj" \
	"$(INTDIR)\vncClient.obj" \
	"$(INTDIR)\vncConnDialog.obj" \
	"$(INTDIR)\vncDesktop.obj" \
	"$(INTDIR)\vncEncodeCoRRE.obj" \
	"$(INTDIR)\vncEncodeHexT.obj" \
	"$(INTDIR)\vncEncoder.obj" \
	"$(INTDIR)\vncEncodeRRE.obj" \
	"$(INTDIR)\vncEncodeTight.obj" \
	"$(INTDIR)\vncEncodeZlib.obj" \
	"$(INTDIR)\vncEncodeZlibHex.obj" \
	"$(INTDIR)\VNCHelp.obj" \
	"$(INTDIR)\vncHTTPConnect.obj" \
	"$(INTDIR)\vncInstHandler.obj" \
	"$(INTDIR)\vncKeymap.obj" \
	"$(INTDIR)\vncMenu.obj" \
	"$(INTDIR)\vncProperties.obj" \
	"$(INTDIR)\vncRegion.obj" \
	"$(INTDIR)\vncServer.obj" \
	"$(INTDIR)\vncService.obj" \
	"$(INTDIR)\vncSockConnect.obj" \
	"$(INTDIR)\vncTimedMsgBox.obj" \
	"$(INTDIR)\VSocket.obj" \
	"$(INTDIR)\WallpaperUtils.obj" \
	"$(INTDIR)\WinVNC.obj" \
	"$(INTDIR)\WinVNC.res" \
	"$(OUTDIR)\VNCHooks.lib" \
	"$(OUTDIR)\omnithread.lib" \
	"$(OUTDIR)\zlib.lib" \
	"$(OUTDIR)\libjpeg.lib"

"$(OUTDIR)\AppShare.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
   cl /c /nologo /Fo.\HorizonLive\ /Fd.\HorizonLive /MT BuildTime.cpp
	 $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("WinVNC.dep")
!INCLUDE "WinVNC.dep"
!ELSE 
!MESSAGE Warning: cannot find "WinVNC.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "WinVNC - Win32 Release" || "$(CFG)" == "WinVNC - Win32 Debug" || "$(CFG)" == "WinVNC - Win32 Profile" || "$(CFG)" == "WinVNC - Win32 HorizonLive"
SOURCE=.\AdministrationControls.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\AdministrationControls.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\AdministrationControls.obj"	"$(INTDIR)\AdministrationControls.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\AdministrationControls.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\AdministrationControls.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\BuildTime.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\BuildTime.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\BuildTime.obj"	"$(INTDIR)\BuildTime.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\BuildTime.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\BuildTime.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\d3des.c

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\d3des.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\d3des.obj"	"$(INTDIR)\d3des.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\d3des.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\d3des.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\DynamicFn.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\DynamicFn.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\DynamicFn.obj"	"$(INTDIR)\DynamicFn.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\DynamicFn.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\DynamicFn.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\FileTransferItemInfo.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\FileTransferItemInfo.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\FileTransferItemInfo.obj"	"$(INTDIR)\FileTransferItemInfo.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\FileTransferItemInfo.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\FileTransferItemInfo.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\IncomingConnectionsControls.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\IncomingConnectionsControls.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\IncomingConnectionsControls.obj"	"$(INTDIR)\IncomingConnectionsControls.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\IncomingConnectionsControls.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\IncomingConnectionsControls.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\InputHandlingControls.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\InputHandlingControls.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\InputHandlingControls.obj"	"$(INTDIR)\InputHandlingControls.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\InputHandlingControls.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\InputHandlingControls.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\Log.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\Log.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\Log.obj"	"$(INTDIR)\Log.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\Log.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\Log.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\MatchWindow.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\MatchWindow.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\MatchWindow.obj"	"$(INTDIR)\MatchWindow.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\MatchWindow.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\MatchWindow.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\MinMax.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\MinMax.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\MinMax.obj"	"$(INTDIR)\MinMax.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\MinMax.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\MinMax.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\ParseHost.c

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\ParseHost.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\ParseHost.obj"	"$(INTDIR)\ParseHost.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\ParseHost.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\ParseHost.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\PollControls.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\PollControls.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\PollControls.obj"	"$(INTDIR)\PollControls.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\PollControls.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\PollControls.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\QuerySettingsControls.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\QuerySettingsControls.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\QuerySettingsControls.obj"	"$(INTDIR)\QuerySettingsControls.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\QuerySettingsControls.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\QuerySettingsControls.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\RectList.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\RectList.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\RectList.obj"	"$(INTDIR)\RectList.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\RectList.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\RectList.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\SharedDesktopArea.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\SharedDesktopArea.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\SharedDesktopArea.obj"	"$(INTDIR)\SharedDesktopArea.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\SharedDesktopArea.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\SharedDesktopArea.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\stdhdrs.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\stdhdrs.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\stdhdrs.obj"	"$(INTDIR)\stdhdrs.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\stdhdrs.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\stdhdrs.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\tableinitcmtemplate.cpp
SOURCE=.\tableinittctemplate.cpp
SOURCE=.\tabletranstemplate.cpp
SOURCE=.\translate.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\translate.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\translate.obj"	"$(INTDIR)\translate.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\translate.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\translate.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\TsSessions.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\TsSessions.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\TsSessions.obj"	"$(INTDIR)\TsSessions.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\TsSessions.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\TsSessions.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\VideoDriver.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\VideoDriver.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\VideoDriver.obj"	"$(INTDIR)\VideoDriver.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\VideoDriver.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\VideoDriver.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncAbout.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncAbout.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncAbout.obj"	"$(INTDIR)\vncAbout.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncAbout.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncAbout.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncAcceptDialog.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncAcceptDialog.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncAcceptDialog.obj"	"$(INTDIR)\vncAcceptDialog.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncAcceptDialog.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncAcceptDialog.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncAcceptReverseDlg.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncAcceptReverseDlg.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncAcceptReverseDlg.obj"	"$(INTDIR)\vncAcceptReverseDlg.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncAcceptReverseDlg.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncAcceptReverseDlg.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncauth.c

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncauth.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncauth.obj"	"$(INTDIR)\vncauth.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncauth.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncauth.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncBuffer.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncBuffer.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncBuffer.obj"	"$(INTDIR)\vncBuffer.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncBuffer.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncBuffer.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncClient.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncClient.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncClient.obj"	"$(INTDIR)\vncClient.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncClient.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncClient.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncConnDialog.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncConnDialog.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncConnDialog.obj"	"$(INTDIR)\vncConnDialog.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncConnDialog.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncConnDialog.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncDesktop.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncDesktop.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncDesktop.obj"	"$(INTDIR)\vncDesktop.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncDesktop.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncDesktop.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncEncodeCoRRE.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncEncodeCoRRE.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncEncodeCoRRE.obj"	"$(INTDIR)\vncEncodeCoRRE.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncEncodeCoRRE.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncEncodeCoRRE.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncEncodeHexT.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncEncodeHexT.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncEncodeHexT.obj"	"$(INTDIR)\vncEncodeHexT.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncEncodeHexT.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncEncodeHexT.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncEncoder.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncEncoder.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncEncoder.obj"	"$(INTDIR)\vncEncoder.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncEncoder.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncEncoder.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncEncodeRRE.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncEncodeRRE.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncEncodeRRE.obj"	"$(INTDIR)\vncEncodeRRE.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncEncodeRRE.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncEncodeRRE.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncEncodeTight.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncEncodeTight.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncEncodeTight.obj"	"$(INTDIR)\vncEncodeTight.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncEncodeTight.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncEncodeTight.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncEncodeZlib.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncEncodeZlib.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncEncodeZlib.obj"	"$(INTDIR)\vncEncodeZlib.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncEncodeZlib.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncEncodeZlib.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncEncodeZlibHex.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncEncodeZlibHex.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncEncodeZlibHex.obj"	"$(INTDIR)\vncEncodeZlibHex.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncEncodeZlibHex.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncEncodeZlibHex.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\VNCHelp.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\VNCHelp.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\VNCHelp.obj"	"$(INTDIR)\VNCHelp.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\VNCHelp.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\VNCHelp.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncHTTPConnect.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncHTTPConnect.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncHTTPConnect.obj"	"$(INTDIR)\vncHTTPConnect.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncHTTPConnect.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncHTTPConnect.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncInstHandler.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncInstHandler.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncInstHandler.obj"	"$(INTDIR)\vncInstHandler.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncInstHandler.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncInstHandler.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncKeymap.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncKeymap.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncKeymap.obj"	"$(INTDIR)\vncKeymap.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncKeymap.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncKeymap.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncMenu.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncMenu.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncMenu.obj"	"$(INTDIR)\vncMenu.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncMenu.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncMenu.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncProperties.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncProperties.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncProperties.obj"	"$(INTDIR)\vncProperties.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncProperties.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncProperties.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncRegion.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncRegion.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncRegion.obj"	"$(INTDIR)\vncRegion.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncRegion.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncRegion.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncServer.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncServer.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncServer.obj"	"$(INTDIR)\vncServer.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncServer.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncServer.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncService.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncService.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncService.obj"	"$(INTDIR)\vncService.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncService.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncService.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncSockConnect.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncSockConnect.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncSockConnect.obj"	"$(INTDIR)\vncSockConnect.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncSockConnect.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncSockConnect.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\vncTimedMsgBox.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\vncTimedMsgBox.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\vncTimedMsgBox.obj"	"$(INTDIR)\vncTimedMsgBox.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\vncTimedMsgBox.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\vncTimedMsgBox.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\VSocket.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\VSocket.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\VSocket.obj"	"$(INTDIR)\VSocket.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\VSocket.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\VSocket.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\WallpaperUtils.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\WallpaperUtils.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\WallpaperUtils.obj"	"$(INTDIR)\WallpaperUtils.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\WallpaperUtils.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\WallpaperUtils.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\WinVNC.cpp

!IF  "$(CFG)" == "WinVNC - Win32 Release"


"$(INTDIR)\WinVNC.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"


"$(INTDIR)\WinVNC.obj"	"$(INTDIR)\WinVNC.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"


"$(INTDIR)\WinVNC.obj" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"


"$(INTDIR)\WinVNC.obj" : $(SOURCE) "$(INTDIR)"


!ENDIF 

SOURCE=.\WinVNC.rc

"$(INTDIR)\WinVNC.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)


!IF  "$(CFG)" == "WinVNC - Win32 Release"

"VNCHooks - Win32 Release" : 
   cd ".\VNCHooks"
   $(MAKE) /$(MAKEFLAGS) /F ".\VNCHooks.mak" CFG="VNCHooks - Win32 Release" 
   cd ".."

"VNCHooks - Win32 ReleaseCLEAN" : 
   cd ".\VNCHooks"
   $(MAKE) /$(MAKEFLAGS) /F ".\VNCHooks.mak" CFG="VNCHooks - Win32 Release" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"

"VNCHooks - Win32 Debug" : 
   cd ".\VNCHooks"
   $(MAKE) /$(MAKEFLAGS) /F ".\VNCHooks.mak" CFG="VNCHooks - Win32 Debug" 
   cd ".."

"VNCHooks - Win32 DebugCLEAN" : 
   cd ".\VNCHooks"
   $(MAKE) /$(MAKEFLAGS) /F ".\VNCHooks.mak" CFG="VNCHooks - Win32 Debug" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"

"VNCHooks - Win32 Profile" : 
   cd ".\VNCHooks"
   $(MAKE) /$(MAKEFLAGS) /F ".\VNCHooks.mak" CFG="VNCHooks - Win32 Profile" 
   cd ".."

"VNCHooks - Win32 ProfileCLEAN" : 
   cd ".\VNCHooks"
   $(MAKE) /$(MAKEFLAGS) /F ".\VNCHooks.mak" CFG="VNCHooks - Win32 Profile" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"

"VNCHooks - Win32 HorizonLive" : 
   cd ".\VNCHooks"
   $(MAKE) /$(MAKEFLAGS) /F ".\VNCHooks.mak" CFG="VNCHooks - Win32 HorizonLive" 
   cd ".."

"VNCHooks - Win32 HorizonLiveCLEAN" : 
   cd ".\VNCHooks"
   $(MAKE) /$(MAKEFLAGS) /F ".\VNCHooks.mak" CFG="VNCHooks - Win32 HorizonLive" RECURSE=1 CLEAN 
   cd ".."

!ENDIF 

!IF  "$(CFG)" == "WinVNC - Win32 Release"

"omnithread - Win32 Release" : 
   cd ".\omnithread"
   $(MAKE) /$(MAKEFLAGS) /F ".\omnithread.mak" CFG="omnithread - Win32 Release" 
   cd ".."

"omnithread - Win32 ReleaseCLEAN" : 
   cd ".\omnithread"
   $(MAKE) /$(MAKEFLAGS) /F ".\omnithread.mak" CFG="omnithread - Win32 Release" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"

"omnithread - Win32 Debug" : 
   cd ".\omnithread"
   $(MAKE) /$(MAKEFLAGS) /F ".\omnithread.mak" CFG="omnithread - Win32 Debug" 
   cd ".."

"omnithread - Win32 DebugCLEAN" : 
   cd ".\omnithread"
   $(MAKE) /$(MAKEFLAGS) /F ".\omnithread.mak" CFG="omnithread - Win32 Debug" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"

"omnithread - Win32 Profile" : 
   cd ".\omnithread"
   $(MAKE) /$(MAKEFLAGS) /F ".\omnithread.mak" CFG="omnithread - Win32 Profile" 
   cd ".."

"omnithread - Win32 ProfileCLEAN" : 
   cd ".\omnithread"
   $(MAKE) /$(MAKEFLAGS) /F ".\omnithread.mak" CFG="omnithread - Win32 Profile" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"

"omnithread - Win32 HorizonLive" : 
   cd ".\omnithread"
   $(MAKE) /$(MAKEFLAGS) /F ".\omnithread.mak" CFG="omnithread - Win32 HorizonLive" 
   cd ".."

"omnithread - Win32 HorizonLiveCLEAN" : 
   cd ".\omnithread"
   $(MAKE) /$(MAKEFLAGS) /F ".\omnithread.mak" CFG="omnithread - Win32 HorizonLive" RECURSE=1 CLEAN 
   cd ".."

!ENDIF 

!IF  "$(CFG)" == "WinVNC - Win32 Release"

"zlib - Win32 Release" : 
   cd ".\zlib"
   $(MAKE) /$(MAKEFLAGS) /F ".\zlib.mak" CFG="zlib - Win32 Release" 
   cd ".."

"zlib - Win32 ReleaseCLEAN" : 
   cd ".\zlib"
   $(MAKE) /$(MAKEFLAGS) /F ".\zlib.mak" CFG="zlib - Win32 Release" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"

"zlib - Win32 Debug" : 
   cd ".\zlib"
   $(MAKE) /$(MAKEFLAGS) /F ".\zlib.mak" CFG="zlib - Win32 Debug" 
   cd ".."

"zlib - Win32 DebugCLEAN" : 
   cd ".\zlib"
   $(MAKE) /$(MAKEFLAGS) /F ".\zlib.mak" CFG="zlib - Win32 Debug" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"

"zlib - Win32 Profile" : 
   cd ".\zlib"
   $(MAKE) /$(MAKEFLAGS) /F ".\zlib.mak" CFG="zlib - Win32 Profile" 
   cd ".."

"zlib - Win32 ProfileCLEAN" : 
   cd ".\zlib"
   $(MAKE) /$(MAKEFLAGS) /F ".\zlib.mak" CFG="zlib - Win32 Profile" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"

"zlib - Win32 HorizonLive" : 
   cd ".\zlib"
   $(MAKE) /$(MAKEFLAGS) /F ".\zlib.mak" CFG="zlib - Win32 HorizonLive" 
   cd ".."

"zlib - Win32 HorizonLiveCLEAN" : 
   cd ".\zlib"
   $(MAKE) /$(MAKEFLAGS) /F ".\zlib.mak" CFG="zlib - Win32 HorizonLive" RECURSE=1 CLEAN 
   cd ".."

!ENDIF 

!IF  "$(CFG)" == "WinVNC - Win32 Release"

"libjpeg - Win32 Release" : 
   cd ".\libjpeg"
   $(MAKE) /$(MAKEFLAGS) /F ".\libjpeg.mak" CFG="libjpeg - Win32 Release" 
   cd ".."

"libjpeg - Win32 ReleaseCLEAN" : 
   cd ".\libjpeg"
   $(MAKE) /$(MAKEFLAGS) /F ".\libjpeg.mak" CFG="libjpeg - Win32 Release" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "WinVNC - Win32 Debug"

"libjpeg - Win32 Debug" : 
   cd ".\libjpeg"
   $(MAKE) /$(MAKEFLAGS) /F ".\libjpeg.mak" CFG="libjpeg - Win32 Debug" 
   cd ".."

"libjpeg - Win32 DebugCLEAN" : 
   cd ".\libjpeg"
   $(MAKE) /$(MAKEFLAGS) /F ".\libjpeg.mak" CFG="libjpeg - Win32 Debug" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "WinVNC - Win32 Profile"

"libjpeg - Win32 Profile" : 
   cd ".\libjpeg"
   $(MAKE) /$(MAKEFLAGS) /F ".\libjpeg.mak" CFG="libjpeg - Win32 Profile" 
   cd ".."

"libjpeg - Win32 ProfileCLEAN" : 
   cd ".\libjpeg"
   $(MAKE) /$(MAKEFLAGS) /F ".\libjpeg.mak" CFG="libjpeg - Win32 Profile" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "WinVNC - Win32 HorizonLive"

"libjpeg - Win32 HorizonLive" : 
   cd ".\libjpeg"
   $(MAKE) /$(MAKEFLAGS) /F ".\libjpeg.mak" CFG="libjpeg - Win32 HorizonLive" 
   cd ".."

"libjpeg - Win32 HorizonLiveCLEAN" : 
   cd ".\libjpeg"
   $(MAKE) /$(MAKEFLAGS) /F ".\libjpeg.mak" CFG="libjpeg - Win32 HorizonLive" RECURSE=1 CLEAN 
   cd ".."

!ENDIF 


!ENDIF 

