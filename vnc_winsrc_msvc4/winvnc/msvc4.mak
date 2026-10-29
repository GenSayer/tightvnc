# WinVNC - Visual C++ 4.x NMAKE file.  Run from this directory; see
# ..\msvc4.inc.
#
# Build ..\vc2ctl and the omnithread, zlib, libjpeg and VNCHooks
# subdirectories first (build.cmd does this).
#
# USE_VC2CTL selects the built-in common controls from ..\vc2ctl, used on
# Windows NT before 3.51 only, and loads COMCTL32 at run time (see
# ..\vc2ctl\vc2gate.h).  That is why comctl32.lib is not linked here.  The
# Developer Studio makefiles do not define USE_VC2CTL and build the original
# code, including its own NT 3.5 workarounds.

!INCLUDE ..\msvc4.inc

CFLAGS=$(CBASE) $(CRTST) /I omnithread /I zlib /I .. /I ..\vc2ctl /D__WIN32__ /D__NT__ /D_WINSTATIC /DNCORBA /DWINVER=0x0400 /DUSE_VC2CTL $(HELPDEF)

LIBS=wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib

SUBLIBS= \
	VNCHooks\$(OUTDIR)\VNCHooks.lib \
	..\vc2ctl\$(OUTDIR)\vc2ctl.lib \
	omnithread\$(OUTDIR)\omnithread.lib \
	zlib\$(OUTDIR)\zlib.lib \
	libjpeg\$(OUTDIR)\libjpeg.lib

OBJS= \
	$(OUTDIR)\AdministrationControls.obj \
	$(OUTDIR)\BuildTime.obj \
	$(OUTDIR)\d3des.obj \
	$(OUTDIR)\DynamicFn.obj \
	$(OUTDIR)\FileTransferItemInfo.obj \
	$(OUTDIR)\IncomingConnectionsControls.obj \
	$(OUTDIR)\InputHandlingControls.obj \
	$(OUTDIR)\Log.obj \
	$(OUTDIR)\MatchWindow.obj \
	$(OUTDIR)\MinMax.obj \
	$(OUTDIR)\ParseHost.obj \
	$(OUTDIR)\PollControls.obj \
	$(OUTDIR)\QuerySettingsControls.obj \
	$(OUTDIR)\RectList.obj \
	$(OUTDIR)\SharedDesktopArea.obj \
	$(OUTDIR)\stdhdrs.obj \
	$(OUTDIR)\translate.obj \
	$(OUTDIR)\TsSessions.obj \
	$(OUTDIR)\VideoDriver.obj \
	$(OUTDIR)\vncAbout.obj \
	$(OUTDIR)\vncAcceptDialog.obj \
	$(OUTDIR)\vncAcceptReverseDlg.obj \
	$(OUTDIR)\vncauth.obj \
	$(OUTDIR)\vncBuffer.obj \
	$(OUTDIR)\vncClient.obj \
	$(OUTDIR)\vncConnDialog.obj \
	$(OUTDIR)\vncDesktop.obj \
	$(OUTDIR)\vncEncodeCoRRE.obj \
	$(OUTDIR)\vncEncodeHexT.obj \
	$(OUTDIR)\vncEncoder.obj \
	$(OUTDIR)\vncEncodeRRE.obj \
	$(OUTDIR)\vncEncodeTight.obj \
	$(OUTDIR)\vncEncodeZlib.obj \
	$(OUTDIR)\vncEncodeZlibHex.obj \
	$(OUTDIR)\VNCHelp.obj \
	$(OUTDIR)\vncHTTPConnect.obj \
	$(OUTDIR)\vncInstHandler.obj \
	$(OUTDIR)\vncKeymap.obj \
	$(OUTDIR)\vncMenu.obj \
	$(OUTDIR)\vncProperties.obj \
	$(OUTDIR)\vncRegion.obj \
	$(OUTDIR)\vncServer.obj \
	$(OUTDIR)\vncService.obj \
	$(OUTDIR)\vncSockConnect.obj \
	$(OUTDIR)\vncTimedMsgBox.obj \
	$(OUTDIR)\VSocket.obj \
	$(OUTDIR)\WallpaperUtils.obj \
	$(OUTDIR)\WinVNC.obj

all: $(OUTDIR) $(OUTDIR)\WinVNC.exe

$(OUTDIR):
	if not exist $(OUTDIR)\nul mkdir $(OUTDIR)

$(OUTDIR)\WinVNC.exe: $(OBJS) $(OUTDIR)\WinVNC.res $(SUBLIBS)
	$(LINK) @<<
$(LBASE) /out:$@
$(OBJS)
$(OUTDIR)\WinVNC.res
$(SUBLIBS)
$(LIBS)
<<

# Set WITH_JAVA_VIEWER= on the command line to leave the Java viewer classes
# out of the resources (smaller EXE, no built-in HTTP viewer).
!IFNDEF WITH_JAVA_VIEWER
WITH_JAVA_VIEWER=1
!ENDIF
!IF "$(WITH_JAVA_VIEWER)" == "1"
RCJAVA=/dWITH_JAVA_VIEWER
!ELSE
RCJAVA=
!ENDIF

$(OUTDIR)\WinVNC.res: WinVNC.rc resource.h
	$(RC) $(RCBASE) /l 0x809 $(RCJAVA) /fo$@ WinVNC.rc

.SUFFIXES: .c .cpp

{.}.cpp{$(OUTDIR)}.obj:
	$(CC) $(CFLAGS) /Fo$(OUTDIR)\ /c $<

{.}.c{$(OUTDIR)}.obj:
	$(CC) $(CFLAGS) /Fo$(OUTDIR)\ /c $<

clean:
	-del $(OUTDIR)\*.obj
	-del $(OUTDIR)\*.res
	-del $(OUTDIR)\WinVNC.exe
	-del $(OUTDIR)\VNCHooks.dll
