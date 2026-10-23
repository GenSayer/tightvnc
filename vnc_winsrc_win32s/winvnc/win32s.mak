# WinVNC - Visual C++ NMAKE file.  Run from this directory; see ..\win32s.inc.
#
# Build omnithread, zlib and libjpeg first (build.cmd does this).
#
# VNCHooks.lib is deliberately NOT linked: VNCHooksStub.cpp loads VNCHooks.dll
# with LoadLibrary on NT and falls back to polling stubs on Win32s, so WinVNC
# still loads where the DLL cannot work.  The NT service, desktop and security
# APIs are resolved the same way (Win32sApi.cpp), which is why advapi32.lib is
# the only security-related import library here.

!INCLUDE ..\win32s.inc

CFLAGS=$(CBASE) $(FORCEINC) /I omnithread /I zlib /I .. /D__WIN32__ /D__NT__ /D_WINSTATIC /DNCORBA

LIBS=wsock32.lib kernel32.lib user32.lib gdi32.lib comdlg32.lib advapi32.lib

SUBLIBS= \
	omnithread\$(OUTDIR)\omnithread.lib \
	zlib\$(OUTDIR)\zlib.lib \
	libjpeg\$(OUTDIR)\libjpeg.lib

OBJS= \
	$(OUTDIR)\AdministrationControls.obj \
	$(OUTDIR)\BuildTime.obj \
	$(OUTDIR)\DynamicFn.obj \
	$(OUTDIR)\FileTransferItemInfo.obj \
	$(OUTDIR)\IncomingConnectionsControls.obj \
	$(OUTDIR)\IniSettings.obj \
	$(OUTDIR)\InputHandlingControls.obj \
	$(OUTDIR)\Log.obj \
	$(OUTDIR)\MatchWindow.obj \
	$(OUTDIR)\MinMax.obj \
	$(OUTDIR)\ParseHost.obj \
	$(OUTDIR)\PollControls.obj \
	$(OUTDIR)\QuerySettingsControls.obj \
	$(OUTDIR)\RectList.obj \
	$(OUTDIR)\SharedDesktopArea.obj \
	$(OUTDIR)\TsSessions.obj \
	$(OUTDIR)\VNCHelp.obj \
	$(OUTDIR)\VNCHooksStub.obj \
	$(OUTDIR)\VSocket.obj \
	$(OUTDIR)\WallpaperUtils.obj \
	$(OUTDIR)\Win32sApi.obj \
	$(OUTDIR)\WinVNC.obj \
	$(OUTDIR)\d3des.obj \
	$(OUTDIR)\stdhdrs.obj \
	$(OUTDIR)\translate.obj \
	$(OUTDIR)\vncAbout.obj \
	$(OUTDIR)\vncAcceptDialog.obj \
	$(OUTDIR)\vncAcceptReverseDlg.obj \
	$(OUTDIR)\vncBuffer.obj \
	$(OUTDIR)\vncClient.obj \
	$(OUTDIR)\vncConnDialog.obj \
	$(OUTDIR)\vncDesktop.obj \
	$(OUTDIR)\vncEncodeCoRRE.obj \
	$(OUTDIR)\vncEncodeHexT.obj \
	$(OUTDIR)\vncEncodeRRE.obj \
	$(OUTDIR)\vncEncodeTight.obj \
	$(OUTDIR)\vncEncodeZlib.obj \
	$(OUTDIR)\vncEncodeZlibHex.obj \
	$(OUTDIR)\vncEncoder.obj \
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
	$(OUTDIR)\vncauth.obj

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
	$(RC) $(RCBASE) $(RCJAVA) /fo$@ WinVNC.rc

.SUFFIXES: .c .cpp

{.}.cpp{$(OUTDIR)}.obj:
	$(CC) $(CFLAGS) /Fo$(OUTDIR)\ /c $<

{.}.c{$(OUTDIR)}.obj:
	$(CC) $(CFLAGS) /Fo$(OUTDIR)\ /c $<

clean:
	-del $(OUTDIR)\*.obj
	-del $(OUTDIR)\*.res
	-del $(OUTDIR)\WinVNC.exe
