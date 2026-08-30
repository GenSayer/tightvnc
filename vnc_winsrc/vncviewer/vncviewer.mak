# Microsoft Developer Studio Generated NMAKE File, Based on vncviewer.dsp
!IF "$(CFG)" == ""
CFG=vncviewer - Win32 Release
!MESSAGE No configuration specified. Defaulting to vncviewer - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "vncviewer - Win32 Release" && "$(CFG)" != "vncviewer - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "vncviewer.mak" CFG="vncviewer - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "vncviewer - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "vncviewer - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "vncviewer - Win32 Release"

OUTDIR=.\Release
INTDIR=.\./Release/vncviewer
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\vncviewer.exe" "$(OUTDIR)\vncviewer.bsc"

!ELSE 

ALL : "libjpeg - Win32 Release" "zlib - Win32 Release" "omnithread - Win32 Release" "$(OUTDIR)\vncviewer.exe" "$(OUTDIR)\vncviewer.bsc"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"omnithread - Win32 ReleaseCLEAN" "zlib - Win32 ReleaseCLEAN" "libjpeg - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\AboutBox.obj"
	-@erase "$(INTDIR)\AboutBox.sbr"
	-@erase "$(INTDIR)\BuildTime.obj"
	-@erase "$(INTDIR)\BuildTime.sbr"
	-@erase "$(INTDIR)\CapsContainer.obj"
	-@erase "$(INTDIR)\CapsContainer.sbr"
	-@erase "$(INTDIR)\ClientConnection.obj"
	-@erase "$(INTDIR)\ClientConnection.sbr"
	-@erase "$(INTDIR)\ClientConnectionClipboard.obj"
	-@erase "$(INTDIR)\ClientConnectionClipboard.sbr"
	-@erase "$(INTDIR)\ClientConnectionCopyRect.obj"
	-@erase "$(INTDIR)\ClientConnectionCopyRect.sbr"
	-@erase "$(INTDIR)\ClientConnectionCoRRE.obj"
	-@erase "$(INTDIR)\ClientConnectionCoRRE.sbr"
	-@erase "$(INTDIR)\ClientConnectionCursor.obj"
	-@erase "$(INTDIR)\ClientConnectionCursor.sbr"
	-@erase "$(INTDIR)\ClientConnectionFile.obj"
	-@erase "$(INTDIR)\ClientConnectionFile.sbr"
	-@erase "$(INTDIR)\ClientConnectionFullScreen.obj"
	-@erase "$(INTDIR)\ClientConnectionFullScreen.sbr"
	-@erase "$(INTDIR)\ClientConnectionHextile.obj"
	-@erase "$(INTDIR)\ClientConnectionHextile.sbr"
	-@erase "$(INTDIR)\ClientConnectionRaw.obj"
	-@erase "$(INTDIR)\ClientConnectionRaw.sbr"
	-@erase "$(INTDIR)\ClientConnectionRRE.obj"
	-@erase "$(INTDIR)\ClientConnectionRRE.sbr"
	-@erase "$(INTDIR)\ClientConnectionTight.obj"
	-@erase "$(INTDIR)\ClientConnectionTight.sbr"
	-@erase "$(INTDIR)\ClientConnectionZlib.obj"
	-@erase "$(INTDIR)\ClientConnectionZlib.sbr"
	-@erase "$(INTDIR)\ClientConnectionZlibHex.obj"
	-@erase "$(INTDIR)\ClientConnectionZlibHex.sbr"
	-@erase "$(INTDIR)\ConnectingDialog.obj"
	-@erase "$(INTDIR)\ConnectingDialog.sbr"
	-@erase "$(INTDIR)\d3des.obj"
	-@erase "$(INTDIR)\d3des.sbr"
	-@erase "$(INTDIR)\Daemon.obj"
	-@erase "$(INTDIR)\Daemon.sbr"
	-@erase "$(INTDIR)\Exception.obj"
	-@erase "$(INTDIR)\Exception.sbr"
	-@erase "$(INTDIR)\FileTransfer.obj"
	-@erase "$(INTDIR)\FileTransfer.sbr"
	-@erase "$(INTDIR)\FileTransferItemInfo.obj"
	-@erase "$(INTDIR)\FileTransferItemInfo.sbr"
	-@erase "$(INTDIR)\HotKeys.obj"
	-@erase "$(INTDIR)\HotKeys.sbr"
	-@erase "$(INTDIR)\KeyMap.obj"
	-@erase "$(INTDIR)\KeyMap.sbr"
	-@erase "$(INTDIR)\Log.obj"
	-@erase "$(INTDIR)\Log.sbr"
	-@erase "$(INTDIR)\LoginAuthDialog.obj"
	-@erase "$(INTDIR)\LoginAuthDialog.sbr"
	-@erase "$(INTDIR)\SessionDialog.obj"
	-@erase "$(INTDIR)\SessionDialog.sbr"
	-@erase "$(INTDIR)\stdhdrs.obj"
	-@erase "$(INTDIR)\stdhdrs.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vncauth.obj"
	-@erase "$(INTDIR)\vncauth.sbr"
	-@erase "$(INTDIR)\VNCHelp.obj"
	-@erase "$(INTDIR)\VNCHelp.sbr"
	-@erase "$(INTDIR)\VNCOptions.obj"
	-@erase "$(INTDIR)\VNCOptions.sbr"
	-@erase "$(INTDIR)\vncviewer.obj"
	-@erase "$(INTDIR)\vncviewer.res"
	-@erase "$(INTDIR)\vncviewer.sbr"
	-@erase "$(INTDIR)\VNCviewerApp.obj"
	-@erase "$(INTDIR)\VNCviewerApp.sbr"
	-@erase "$(INTDIR)\VNCviewerApp32.obj"
	-@erase "$(INTDIR)\VNCviewerApp32.sbr"
	-@erase "$(OUTDIR)\vncviewer.bsc"
	-@erase "$(OUTDIR)\vncviewer.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /Ob0 /I "omnithread" /I ".." /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_WIN32_IE=0x600" /D "__NT__" /D "_WINSTATIC" /D "__WIN32__" /D "XMD_H" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\vncviewer.pch" /YX"stdhdrs.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\vncviewer.res" /d "NDEBUG" 
BSC32_SBRS= \
	"$(INTDIR)\AboutBox.sbr" \
	"$(INTDIR)\BuildTime.sbr" \
	"$(INTDIR)\CapsContainer.sbr" \
	"$(INTDIR)\ClientConnection.sbr" \
	"$(INTDIR)\ClientConnectionClipboard.sbr" \
	"$(INTDIR)\ClientConnectionCopyRect.sbr" \
	"$(INTDIR)\ClientConnectionCoRRE.sbr" \
	"$(INTDIR)\ClientConnectionCursor.sbr" \
	"$(INTDIR)\ClientConnectionFile.sbr" \
	"$(INTDIR)\ClientConnectionFullScreen.sbr" \
	"$(INTDIR)\ClientConnectionHextile.sbr" \
	"$(INTDIR)\ClientConnectionRaw.sbr" \
	"$(INTDIR)\ClientConnectionRRE.sbr" \
	"$(INTDIR)\ClientConnectionTight.sbr" \
	"$(INTDIR)\ClientConnectionZlib.sbr" \
	"$(INTDIR)\ClientConnectionZlibHex.sbr" \
	"$(INTDIR)\ConnectingDialog.sbr" \
	"$(INTDIR)\d3des.sbr" \
	"$(INTDIR)\Daemon.sbr" \
	"$(INTDIR)\Exception.sbr" \
	"$(INTDIR)\FileTransfer.sbr" \
	"$(INTDIR)\FileTransferItemInfo.sbr" \
	"$(INTDIR)\HotKeys.sbr" \
	"$(INTDIR)\KeyMap.sbr" \
	"$(INTDIR)\Log.sbr" \
	"$(INTDIR)\LoginAuthDialog.sbr" \
	"$(INTDIR)\SessionDialog.sbr" \
	"$(INTDIR)\stdhdrs.sbr" \
	"$(INTDIR)\vncauth.sbr" \
	"$(INTDIR)\VNCHelp.sbr" \
	"$(INTDIR)\VNCOptions.sbr" \
	"$(INTDIR)\vncviewer.sbr" \
	"$(INTDIR)\VNCviewerApp.sbr" \
	"$(INTDIR)\VNCviewerApp32.sbr"

"$(OUTDIR)\vncviewer.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib uuid.lib wsock32.lib comctl32.lib /nologo /subsystem:windows /incremental:no /pdb:"$(OUTDIR)\vncviewer.pdb" /machine:ARM /out:"$(OUTDIR)\vncviewer.exe" 
LINK32_OBJS= \
	"$(INTDIR)\AboutBox.obj" \
	"$(INTDIR)\BuildTime.obj" \
	"$(INTDIR)\CapsContainer.obj" \
	"$(INTDIR)\ClientConnection.obj" \
	"$(INTDIR)\ClientConnectionClipboard.obj" \
	"$(INTDIR)\ClientConnectionCopyRect.obj" \
	"$(INTDIR)\ClientConnectionCoRRE.obj" \
	"$(INTDIR)\ClientConnectionCursor.obj" \
	"$(INTDIR)\ClientConnectionFile.obj" \
	"$(INTDIR)\ClientConnectionFullScreen.obj" \
	"$(INTDIR)\ClientConnectionHextile.obj" \
	"$(INTDIR)\ClientConnectionRaw.obj" \
	"$(INTDIR)\ClientConnectionRRE.obj" \
	"$(INTDIR)\ClientConnectionTight.obj" \
	"$(INTDIR)\ClientConnectionZlib.obj" \
	"$(INTDIR)\ClientConnectionZlibHex.obj" \
	"$(INTDIR)\ConnectingDialog.obj" \
	"$(INTDIR)\d3des.obj" \
	"$(INTDIR)\Daemon.obj" \
	"$(INTDIR)\Exception.obj" \
	"$(INTDIR)\FileTransfer.obj" \
	"$(INTDIR)\FileTransferItemInfo.obj" \
	"$(INTDIR)\HotKeys.obj" \
	"$(INTDIR)\KeyMap.obj" \
	"$(INTDIR)\Log.obj" \
	"$(INTDIR)\LoginAuthDialog.obj" \
	"$(INTDIR)\SessionDialog.obj" \
	"$(INTDIR)\stdhdrs.obj" \
	"$(INTDIR)\vncauth.obj" \
	"$(INTDIR)\VNCHelp.obj" \
	"$(INTDIR)\VNCOptions.obj" \
	"$(INTDIR)\vncviewer.obj" \
	"$(INTDIR)\VNCviewerApp.obj" \
	"$(INTDIR)\VNCviewerApp32.obj" \
	"$(INTDIR)\vncviewer.res" \
	"$(OUTDIR)\omnithread.lib" \
	"$(OUTDIR)\zlib.lib" \
	"$(OUTDIR)\libjpeg.lib"

"$(OUTDIR)\vncviewer.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
   cl /c /nologo /Fo.\Release\vncviewer\ /Fd.\Release\vncviewer /MT BuildTime.cpp
	 $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"

!ELSEIF  "$(CFG)" == "vncviewer - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\./Debug/vncviewer
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\vncviewer.exe" "$(OUTDIR)\vncviewer\vncviewer.pch" "$(OUTDIR)\vncviewer.bsc"

!ELSE 

ALL : "libjpeg - Win32 Debug" "zlib - Win32 Debug" "omnithread - Win32 Debug" "$(OUTDIR)\vncviewer.exe" "$(OUTDIR)\vncviewer\vncviewer.pch" "$(OUTDIR)\vncviewer.bsc"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"omnithread - Win32 DebugCLEAN" "zlib - Win32 DebugCLEAN" "libjpeg - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\AboutBox.obj"
	-@erase "$(INTDIR)\AboutBox.sbr"
	-@erase "$(INTDIR)\BuildTime.obj"
	-@erase "$(INTDIR)\BuildTime.sbr"
	-@erase "$(INTDIR)\CapsContainer.obj"
	-@erase "$(INTDIR)\CapsContainer.sbr"
	-@erase "$(INTDIR)\ClientConnection.obj"
	-@erase "$(INTDIR)\ClientConnection.sbr"
	-@erase "$(INTDIR)\ClientConnectionClipboard.obj"
	-@erase "$(INTDIR)\ClientConnectionClipboard.sbr"
	-@erase "$(INTDIR)\ClientConnectionCopyRect.obj"
	-@erase "$(INTDIR)\ClientConnectionCopyRect.sbr"
	-@erase "$(INTDIR)\ClientConnectionCoRRE.obj"
	-@erase "$(INTDIR)\ClientConnectionCoRRE.sbr"
	-@erase "$(INTDIR)\ClientConnectionCursor.obj"
	-@erase "$(INTDIR)\ClientConnectionCursor.sbr"
	-@erase "$(INTDIR)\ClientConnectionFile.obj"
	-@erase "$(INTDIR)\ClientConnectionFile.sbr"
	-@erase "$(INTDIR)\ClientConnectionFullScreen.obj"
	-@erase "$(INTDIR)\ClientConnectionFullScreen.sbr"
	-@erase "$(INTDIR)\ClientConnectionHextile.obj"
	-@erase "$(INTDIR)\ClientConnectionHextile.sbr"
	-@erase "$(INTDIR)\ClientConnectionRaw.obj"
	-@erase "$(INTDIR)\ClientConnectionRaw.sbr"
	-@erase "$(INTDIR)\ClientConnectionRRE.obj"
	-@erase "$(INTDIR)\ClientConnectionRRE.sbr"
	-@erase "$(INTDIR)\ClientConnectionTight.obj"
	-@erase "$(INTDIR)\ClientConnectionTight.sbr"
	-@erase "$(INTDIR)\ClientConnectionZlib.obj"
	-@erase "$(INTDIR)\ClientConnectionZlib.sbr"
	-@erase "$(INTDIR)\ClientConnectionZlibHex.obj"
	-@erase "$(INTDIR)\ClientConnectionZlibHex.sbr"
	-@erase "$(INTDIR)\ConnectingDialog.obj"
	-@erase "$(INTDIR)\ConnectingDialog.sbr"
	-@erase "$(INTDIR)\d3des.obj"
	-@erase "$(INTDIR)\d3des.sbr"
	-@erase "$(INTDIR)\Daemon.obj"
	-@erase "$(INTDIR)\Daemon.sbr"
	-@erase "$(INTDIR)\Exception.obj"
	-@erase "$(INTDIR)\Exception.sbr"
	-@erase "$(INTDIR)\FileTransfer.obj"
	-@erase "$(INTDIR)\FileTransfer.sbr"
	-@erase "$(INTDIR)\FileTransferItemInfo.obj"
	-@erase "$(INTDIR)\FileTransferItemInfo.sbr"
	-@erase "$(INTDIR)\HotKeys.obj"
	-@erase "$(INTDIR)\HotKeys.sbr"
	-@erase "$(INTDIR)\KeyMap.obj"
	-@erase "$(INTDIR)\KeyMap.sbr"
	-@erase "$(INTDIR)\Log.obj"
	-@erase "$(INTDIR)\Log.sbr"
	-@erase "$(INTDIR)\LoginAuthDialog.obj"
	-@erase "$(INTDIR)\LoginAuthDialog.sbr"
	-@erase "$(INTDIR)\SessionDialog.obj"
	-@erase "$(INTDIR)\SessionDialog.sbr"
	-@erase "$(INTDIR)\stdhdrs.obj"
	-@erase "$(INTDIR)\stdhdrs.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\vncauth.obj"
	-@erase "$(INTDIR)\vncauth.sbr"
	-@erase "$(INTDIR)\VNCHelp.obj"
	-@erase "$(INTDIR)\VNCHelp.sbr"
	-@erase "$(INTDIR)\VNCOptions.obj"
	-@erase "$(INTDIR)\VNCOptions.sbr"
	-@erase "$(INTDIR)\vncviewer.obj"
	-@erase "$(INTDIR)\vncviewer.pch"
	-@erase "$(INTDIR)\vncviewer.res"
	-@erase "$(INTDIR)\vncviewer.sbr"
	-@erase "$(INTDIR)\VNCviewerApp.obj"
	-@erase "$(INTDIR)\VNCviewerApp.sbr"
	-@erase "$(INTDIR)\VNCviewerApp32.obj"
	-@erase "$(INTDIR)\VNCviewerApp32.sbr"
	-@erase "$(OUTDIR)\vncviewer.bsc"
	-@erase "$(OUTDIR)\vncviewer.exe"
	-@erase "$(OUTDIR)\vncviewer.ilk"
	-@erase "$(OUTDIR)\vncviewer.pdb"
	-@erase "$(OUTDIR)\vncviewer\vncviewer.map"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

"$(INTDIR)" :
    if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od /I "omnithread" /I ".." /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_WIN32_IE=0x600" /D "__NT__" /D "_WINSTATIC" /D "__WIN32__" /D "XMD_H" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\vncviewer.pch" /YX"stdhdrs.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\vncviewer.res" /d "_DEBUG" 
BSC32_SBRS= \
	"$(INTDIR)\AboutBox.sbr" \
	"$(INTDIR)\BuildTime.sbr" \
	"$(INTDIR)\CapsContainer.sbr" \
	"$(INTDIR)\ClientConnection.sbr" \
	"$(INTDIR)\ClientConnectionClipboard.sbr" \
	"$(INTDIR)\ClientConnectionCopyRect.sbr" \
	"$(INTDIR)\ClientConnectionCoRRE.sbr" \
	"$(INTDIR)\ClientConnectionCursor.sbr" \
	"$(INTDIR)\ClientConnectionFile.sbr" \
	"$(INTDIR)\ClientConnectionFullScreen.sbr" \
	"$(INTDIR)\ClientConnectionHextile.sbr" \
	"$(INTDIR)\ClientConnectionRaw.sbr" \
	"$(INTDIR)\ClientConnectionRRE.sbr" \
	"$(INTDIR)\ClientConnectionTight.sbr" \
	"$(INTDIR)\ClientConnectionZlib.sbr" \
	"$(INTDIR)\ClientConnectionZlibHex.sbr" \
	"$(INTDIR)\ConnectingDialog.sbr" \
	"$(INTDIR)\d3des.sbr" \
	"$(INTDIR)\Daemon.sbr" \
	"$(INTDIR)\Exception.sbr" \
	"$(INTDIR)\FileTransfer.sbr" \
	"$(INTDIR)\FileTransferItemInfo.sbr" \
	"$(INTDIR)\HotKeys.sbr" \
	"$(INTDIR)\KeyMap.sbr" \
	"$(INTDIR)\Log.sbr" \
	"$(INTDIR)\LoginAuthDialog.sbr" \
	"$(INTDIR)\SessionDialog.sbr" \
	"$(INTDIR)\stdhdrs.sbr" \
	"$(INTDIR)\vncauth.sbr" \
	"$(INTDIR)\VNCHelp.sbr" \
	"$(INTDIR)\VNCOptions.sbr" \
	"$(INTDIR)\vncviewer.sbr" \
	"$(INTDIR)\VNCviewerApp.sbr" \
	"$(INTDIR)\VNCviewerApp32.sbr"

"$(OUTDIR)\vncviewer.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib uuid.lib wsock32.lib comctl32.lib /nologo /subsystem:windows /incremental:yes /pdb:"$(OUTDIR)\vncviewer.pdb" /map:"$(INTDIR)\vncviewer.map" /debug /machine:ARM /out:"$(OUTDIR)\vncviewer.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\AboutBox.obj" \
	"$(INTDIR)\BuildTime.obj" \
	"$(INTDIR)\CapsContainer.obj" \
	"$(INTDIR)\ClientConnection.obj" \
	"$(INTDIR)\ClientConnectionClipboard.obj" \
	"$(INTDIR)\ClientConnectionCopyRect.obj" \
	"$(INTDIR)\ClientConnectionCoRRE.obj" \
	"$(INTDIR)\ClientConnectionCursor.obj" \
	"$(INTDIR)\ClientConnectionFile.obj" \
	"$(INTDIR)\ClientConnectionFullScreen.obj" \
	"$(INTDIR)\ClientConnectionHextile.obj" \
	"$(INTDIR)\ClientConnectionRaw.obj" \
	"$(INTDIR)\ClientConnectionRRE.obj" \
	"$(INTDIR)\ClientConnectionTight.obj" \
	"$(INTDIR)\ClientConnectionZlib.obj" \
	"$(INTDIR)\ClientConnectionZlibHex.obj" \
	"$(INTDIR)\ConnectingDialog.obj" \
	"$(INTDIR)\d3des.obj" \
	"$(INTDIR)\Daemon.obj" \
	"$(INTDIR)\Exception.obj" \
	"$(INTDIR)\FileTransfer.obj" \
	"$(INTDIR)\FileTransferItemInfo.obj" \
	"$(INTDIR)\HotKeys.obj" \
	"$(INTDIR)\KeyMap.obj" \
	"$(INTDIR)\Log.obj" \
	"$(INTDIR)\LoginAuthDialog.obj" \
	"$(INTDIR)\SessionDialog.obj" \
	"$(INTDIR)\stdhdrs.obj" \
	"$(INTDIR)\vncauth.obj" \
	"$(INTDIR)\VNCHelp.obj" \
	"$(INTDIR)\VNCOptions.obj" \
	"$(INTDIR)\vncviewer.obj" \
	"$(INTDIR)\VNCviewerApp.obj" \
	"$(INTDIR)\VNCviewerApp32.obj" \
	"$(INTDIR)\vncviewer.res" \
	"$(OUTDIR)\omnithread.lib" \
	"$(OUTDIR)\zlib.lib" \
	"$(OUTDIR)\libjpeg.lib"

"$(OUTDIR)\vncviewer.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
   cl /c /nologo /Fo.\Debug\vncviewer\ /Fd.\Debug\vncviewer /MTd BuildTime.cpp
	 $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

SOURCE="$(InputPath)"

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("vncviewer.dep")
!INCLUDE "vncviewer.dep"
!ELSE 
!MESSAGE Warning: cannot find "vncviewer.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "vncviewer - Win32 Release" || "$(CFG)" == "vncviewer - Win32 Debug"

!IF  "$(CFG)" == "vncviewer - Win32 Release"

"omnithread - Win32 Release" : 
   cd ".\omnithread"
   $(MAKE) /$(MAKEFLAGS) /F ".\omnithread.mak" CFG="omnithread - Win32 Release" 
   cd ".."

"omnithread - Win32 ReleaseCLEAN" : 
   cd ".\omnithread"
   $(MAKE) /$(MAKEFLAGS) /F ".\omnithread.mak" CFG="omnithread - Win32 Release" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "vncviewer - Win32 Debug"

"omnithread - Win32 Debug" : 
   cd ".\omnithread"
   $(MAKE) /$(MAKEFLAGS) /F ".\omnithread.mak" CFG="omnithread - Win32 Debug" 
   cd ".."

"omnithread - Win32 DebugCLEAN" : 
   cd ".\omnithread"
   $(MAKE) /$(MAKEFLAGS) /F ".\omnithread.mak" CFG="omnithread - Win32 Debug" RECURSE=1 CLEAN 
   cd ".."

!ENDIF 

!IF  "$(CFG)" == "vncviewer - Win32 Release"

"zlib - Win32 Release" : 
   cd ".\zlib"
   $(MAKE) /$(MAKEFLAGS) /F ".\zlib.mak" CFG="zlib - Win32 Release" 
   cd ".."

"zlib - Win32 ReleaseCLEAN" : 
   cd ".\zlib"
   $(MAKE) /$(MAKEFLAGS) /F ".\zlib.mak" CFG="zlib - Win32 Release" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "vncviewer - Win32 Debug"

"zlib - Win32 Debug" : 
   cd ".\zlib"
   $(MAKE) /$(MAKEFLAGS) /F ".\zlib.mak" CFG="zlib - Win32 Debug" 
   cd ".."

"zlib - Win32 DebugCLEAN" : 
   cd ".\zlib"
   $(MAKE) /$(MAKEFLAGS) /F ".\zlib.mak" CFG="zlib - Win32 Debug" RECURSE=1 CLEAN 
   cd ".."

!ENDIF 

!IF  "$(CFG)" == "vncviewer - Win32 Release"

"libjpeg - Win32 Release" : 
   cd ".\libjpeg"
   $(MAKE) /$(MAKEFLAGS) /F ".\libjpeg.mak" CFG="libjpeg - Win32 Release" 
   cd ".."

"libjpeg - Win32 ReleaseCLEAN" : 
   cd ".\libjpeg"
   $(MAKE) /$(MAKEFLAGS) /F ".\libjpeg.mak" CFG="libjpeg - Win32 Release" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "vncviewer - Win32 Debug"

"libjpeg - Win32 Debug" : 
   cd ".\libjpeg"
   $(MAKE) /$(MAKEFLAGS) /F ".\libjpeg.mak" CFG="libjpeg - Win32 Debug" 
   cd ".."

"libjpeg - Win32 DebugCLEAN" : 
   cd ".\libjpeg"
   $(MAKE) /$(MAKEFLAGS) /F ".\libjpeg.mak" CFG="libjpeg - Win32 Debug" RECURSE=1 CLEAN 
   cd ".."

!ENDIF 

SOURCE=.\AboutBox.cpp

"$(INTDIR)\AboutBox.obj"	"$(INTDIR)\AboutBox.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\BuildTime.cpp

"$(INTDIR)\BuildTime.obj"	"$(INTDIR)\BuildTime.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\CapsContainer.cpp

"$(INTDIR)\CapsContainer.obj"	"$(INTDIR)\CapsContainer.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ClientConnection.cpp

"$(INTDIR)\ClientConnection.obj"	"$(INTDIR)\ClientConnection.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ClientConnectionClipboard.cpp

"$(INTDIR)\ClientConnectionClipboard.obj"	"$(INTDIR)\ClientConnectionClipboard.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ClientConnectionCopyRect.cpp

"$(INTDIR)\ClientConnectionCopyRect.obj"	"$(INTDIR)\ClientConnectionCopyRect.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ClientConnectionCoRRE.cpp

"$(INTDIR)\ClientConnectionCoRRE.obj"	"$(INTDIR)\ClientConnectionCoRRE.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ClientConnectionCursor.cpp

"$(INTDIR)\ClientConnectionCursor.obj"	"$(INTDIR)\ClientConnectionCursor.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ClientConnectionFile.cpp

"$(INTDIR)\ClientConnectionFile.obj"	"$(INTDIR)\ClientConnectionFile.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ClientConnectionFullScreen.cpp

"$(INTDIR)\ClientConnectionFullScreen.obj"	"$(INTDIR)\ClientConnectionFullScreen.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ClientConnectionHextile.cpp

"$(INTDIR)\ClientConnectionHextile.obj"	"$(INTDIR)\ClientConnectionHextile.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ClientConnectionRaw.cpp

"$(INTDIR)\ClientConnectionRaw.obj"	"$(INTDIR)\ClientConnectionRaw.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ClientConnectionRRE.cpp

"$(INTDIR)\ClientConnectionRRE.obj"	"$(INTDIR)\ClientConnectionRRE.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ClientConnectionTight.cpp

"$(INTDIR)\ClientConnectionTight.obj"	"$(INTDIR)\ClientConnectionTight.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ClientConnectionZlib.cpp

"$(INTDIR)\ClientConnectionZlib.obj"	"$(INTDIR)\ClientConnectionZlib.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ClientConnectionZlibHex.cpp

"$(INTDIR)\ClientConnectionZlibHex.obj"	"$(INTDIR)\ClientConnectionZlibHex.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ConnectingDialog.cpp

"$(INTDIR)\ConnectingDialog.obj"	"$(INTDIR)\ConnectingDialog.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\d3des.c

"$(INTDIR)\d3des.obj"	"$(INTDIR)\d3des.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Daemon.cpp

"$(INTDIR)\Daemon.obj"	"$(INTDIR)\Daemon.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Exception.cpp

"$(INTDIR)\Exception.obj"	"$(INTDIR)\Exception.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\FileTransfer.cpp

"$(INTDIR)\FileTransfer.obj"	"$(INTDIR)\FileTransfer.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\FileTransferItemInfo.cpp

"$(INTDIR)\FileTransferItemInfo.obj"	"$(INTDIR)\FileTransferItemInfo.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\HotKeys.cpp

"$(INTDIR)\HotKeys.obj"	"$(INTDIR)\HotKeys.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\KeyMap.cpp

"$(INTDIR)\KeyMap.obj"	"$(INTDIR)\KeyMap.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Log.cpp

"$(INTDIR)\Log.obj"	"$(INTDIR)\Log.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\LoginAuthDialog.cpp

"$(INTDIR)\LoginAuthDialog.obj"	"$(INTDIR)\LoginAuthDialog.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\SessionDialog.cpp

"$(INTDIR)\SessionDialog.obj"	"$(INTDIR)\SessionDialog.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\stdhdrs.cpp

!IF  "$(CFG)" == "vncviewer - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /Ob0 /I "omnithread" /I ".." /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_WIN32_IE=0x600" /D "__NT__" /D "_WINSTATIC" /D "__WIN32__" /D "XMD_H" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\vncviewer.pch" /YX"stdhdrs.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\stdhdrs.obj"	"$(INTDIR)\stdhdrs.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "vncviewer - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /GX /ZI /Od /I "omnithread" /I ".." /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_WIN32_IE=0x600" /D "_WINDOWS" /D "__NT__" /D "_WINSTATIC" /D "__WIN32__" /D "XMD_H" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\vncviewer.pch" /Yc"stdhdrs.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\stdhdrs.obj"	"$(INTDIR)\stdhdrs.sbr"	"$(INTDIR)\vncviewer.pch" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\vncauth.c

"$(INTDIR)\vncauth.obj"	"$(INTDIR)\vncauth.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\VNCHelp.cpp

"$(INTDIR)\VNCHelp.obj"	"$(INTDIR)\VNCHelp.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\VNCOptions.cpp

"$(INTDIR)\VNCOptions.obj"	"$(INTDIR)\VNCOptions.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\vncviewer.cpp

"$(INTDIR)\vncviewer.obj"	"$(INTDIR)\vncviewer.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\res\vncviewer.rc

!IF  "$(CFG)" == "vncviewer - Win32 Release"


"$(INTDIR)\vncviewer.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) /l 0x409 /fo"$(INTDIR)\vncviewer.res" /i "res" /d "NDEBUG" $(SOURCE)


!ELSEIF  "$(CFG)" == "vncviewer - Win32 Debug"


"$(INTDIR)\vncviewer.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) /l 0x409 /fo"$(INTDIR)\vncviewer.res" /i "res" /d "_DEBUG" $(SOURCE)


!ENDIF 

SOURCE=.\VNCviewerApp.cpp

"$(INTDIR)\VNCviewerApp.obj"	"$(INTDIR)\VNCviewerApp.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\VNCviewerApp32.cpp

"$(INTDIR)\VNCviewerApp32.obj"	"$(INTDIR)\VNCviewerApp32.sbr" : $(SOURCE) "$(INTDIR)"



!ENDIF 

