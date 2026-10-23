# vncviewer - Visual C++ NMAKE file.  Run from this directory; see ..\win32s.inc.
#
# Build omnithread, zlib and libjpeg first (build.cmd does this).  The import
# list is deliberately the Win32s-safe set: everything newer is resolved at
# run time by Win32sApi.cpp, so the same EXE loads on NT 3.1 and Win32s.

!INCLUDE ..\win32s.inc

# /Ob0 as in the original release build.
CFLAGS=$(CBASE) /Ob0 $(FORCEINC) /I omnithread /I .. /D_WINSTATIC /D__WIN32__ /DXMD_H /DNEED_FAR_POINTERS

LIBS=kernel32.lib user32.lib gdi32.lib comdlg32.lib advapi32.lib wsock32.lib

SUBLIBS= \
	omnithread\$(OUTDIR)\omnithread.lib \
	zlib\$(OUTDIR)\zlib.lib \
	libjpeg\$(OUTDIR)\libjpeg.lib

OBJS= \
	$(OUTDIR)\AboutBox.obj \
	$(OUTDIR)\BuildTime.obj \
	$(OUTDIR)\CapsContainer.obj \
	$(OUTDIR)\ClientConnection.obj \
	$(OUTDIR)\ClientConnectionClipboard.obj \
	$(OUTDIR)\ClientConnectionCoRRE.obj \
	$(OUTDIR)\ClientConnectionCopyRect.obj \
	$(OUTDIR)\ClientConnectionCursor.obj \
	$(OUTDIR)\ClientConnectionFile.obj \
	$(OUTDIR)\ClientConnectionFullScreen.obj \
	$(OUTDIR)\ClientConnectionHextile.obj \
	$(OUTDIR)\ClientConnectionRRE.obj \
	$(OUTDIR)\ClientConnectionRaw.obj \
	$(OUTDIR)\ClientConnectionTight.obj \
	$(OUTDIR)\ClientConnectionZlib.obj \
	$(OUTDIR)\ClientConnectionZlibHex.obj \
	$(OUTDIR)\ConnectingDialog.obj \
	$(OUTDIR)\Daemon.obj \
	$(OUTDIR)\Exception.obj \
	$(OUTDIR)\FileTransfer.obj \
	$(OUTDIR)\FileTransferItemInfo.obj \
	$(OUTDIR)\HotKeys.obj \
	$(OUTDIR)\KeyMap.obj \
	$(OUTDIR)\Log.obj \
	$(OUTDIR)\LoginAuthDialog.obj \
	$(OUTDIR)\SessionDialog.obj \
	$(OUTDIR)\VNCHelp.obj \
	$(OUTDIR)\VNCOptions.obj \
	$(OUTDIR)\VNCviewerApp.obj \
	$(OUTDIR)\VNCviewerApp32.obj \
	$(OUTDIR)\Win32sApi.obj \
	$(OUTDIR)\d3des.obj \
	$(OUTDIR)\stdhdrs.obj \
	$(OUTDIR)\vncauth.obj \
	$(OUTDIR)\vncviewer.obj

all: $(OUTDIR) $(OUTDIR)\vncviewer.exe

$(OUTDIR):
	if not exist $(OUTDIR)\nul mkdir $(OUTDIR)

$(OUTDIR)\vncviewer.exe: $(OBJS) $(OUTDIR)\vncviewer.res $(SUBLIBS)
	$(LINK) @<<
$(LBASE) /out:$@
$(OBJS)
$(OUTDIR)\vncviewer.res
$(SUBLIBS)
$(LIBS)
<<

$(OUTDIR)\vncviewer.res: res\vncviewer.rc res\resource.h
	$(RC) $(RCBASE) /i res /fo$@ res\vncviewer.rc

.SUFFIXES: .c .cpp

{.}.cpp{$(OUTDIR)}.obj:
	$(CC) $(CFLAGS) /Fo$(OUTDIR)\ /c $<

{.}.c{$(OUTDIR)}.obj:
	$(CC) $(CFLAGS) /Fo$(OUTDIR)\ /c $<

clean:
	-del $(OUTDIR)\*.obj
	-del $(OUTDIR)\*.res
	-del $(OUTDIR)\vncviewer.exe
