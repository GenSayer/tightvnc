# WinVNC - NMAKE file.  Run from this directory; see
# ..\nt.inc.
#
# Build the omnithread, zlib, libjpeg and VNCHooks subdirectories first
# (build.cmd does this).  The flags, defines and libraries are those of
# WinVNC.mak (and WinVNC-axp64.mak / WinVNC-ia64.mak for those CPUs).

!INCLUDE ..\nt.inc

!IF "$(HTMLHELP)" == "1"
!IF "$(HHWARN)" == "1"
!MESSAGE Warning: HTMLHELP=1 for CPU=$(CPU).  The HtmlHelp calls crash on IA64 before Windows Server 2003 SP1, and AXP64 is an Alpha-only build that may be unstable.
!ENDIF
!ENDIF

CFLAGS=$(CBASE) $(CRTST) /I omnithread /I zlib /I .. /D__WIN32__ /D__NT__ /D_WINSTATIC /DNCORBA /DWINVER=$(WINVER)

LIBS=wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comctl32.lib $(HHLIB)

SUBLIBS= \
	VNCHooks\$(OUTDIR)\VNCHooks.lib \
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
