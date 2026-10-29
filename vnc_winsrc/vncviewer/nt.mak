# vncviewer - NMAKE file.  Run from this directory; see
# ..\nt.inc.
#
# Build the omnithread, zlib and libjpeg subdirectories first (build.cmd does
# this).  The flags, defines and libraries are those of vncviewer.mak.

!INCLUDE ..\nt.inc

# /Ob0 as in the original release build.
CFLAGS=$(CBASE) $(CRTST) /Ob0 /I omnithread /I .. /D__NT__ /D_WINSTATIC /D__WIN32__ /DXMD_H /DNEED_FAR_POINTERS $(HELPDEF)

LIBS=winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib uuid.lib wsock32.lib comctl32.lib $(HHLIB)

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
	$(OUTDIR)\ClientConnectionCopyRect.obj \
	$(OUTDIR)\ClientConnectionCoRRE.obj \
	$(OUTDIR)\ClientConnectionCursor.obj \
	$(OUTDIR)\ClientConnectionFile.obj \
	$(OUTDIR)\ClientConnectionFullScreen.obj \
	$(OUTDIR)\ClientConnectionHextile.obj \
	$(OUTDIR)\ClientConnectionRaw.obj \
	$(OUTDIR)\ClientConnectionRRE.obj \
	$(OUTDIR)\ClientConnectionTight.obj \
	$(OUTDIR)\ClientConnectionZlib.obj \
	$(OUTDIR)\ClientConnectionZlibHex.obj \
	$(OUTDIR)\ConnectingDialog.obj \
	$(OUTDIR)\d3des.obj \
	$(OUTDIR)\Daemon.obj \
	$(OUTDIR)\Exception.obj \
	$(OUTDIR)\FileTransfer.obj \
	$(OUTDIR)\FileTransferItemInfo.obj \
	$(OUTDIR)\HotKeys.obj \
	$(OUTDIR)\KeyMap.obj \
	$(OUTDIR)\Log.obj \
	$(OUTDIR)\LoginAuthDialog.obj \
	$(OUTDIR)\SessionDialog.obj \
	$(OUTDIR)\stdhdrs.obj \
	$(OUTDIR)\vncauth.obj \
	$(OUTDIR)\VNCHelp.obj \
	$(OUTDIR)\VNCOptions.obj \
	$(OUTDIR)\vncviewer.obj \
	$(OUTDIR)\VNCviewerApp.obj \
	$(OUTDIR)\VNCviewerApp32.obj

all: $(OUTDIR) $(OUTDIR)\vncviewer.exe

$(OUTDIR):
	if not exist $(OUTDIR)\nul mkdir $(OUTDIR)

$(OUTDIR)\vncviewer.exe: $(OBJS) $(OUTDIR)\vncviewer.res $(VSRES) $(SUBLIBS)
	$(LINK) @<<
$(LBASE) /out:$@
$(OBJS)
$(OUTDIR)\vncviewer.res
$(VSRES)
$(SUBLIBS)
$(LIBS)
<<

$(OUTDIR)\vncviewer.res: res\vncviewer.rc res\resource.h
	$(RC) $(RCBASE) /l 0x409 /i res /fo$@ res\vncviewer.rc

$(OUTDIR)\visualstyles.res: ..\visualstyles.rc ..\visualstyles.manifest
	$(RC) $(RCBASE) /i .. /fo$@ ..\visualstyles.rc

.SUFFIXES: .c .cpp

{.}.cpp{$(OUTDIR)}.obj:
	$(CC) $(CFLAGS) /Fo$(OUTDIR)\ /c $<

{.}.c{$(OUTDIR)}.obj:
	$(CC) $(CFLAGS) /Fo$(OUTDIR)\ /c $<

clean:
	-del $(OUTDIR)\*.obj
	-del $(OUTDIR)\*.res
	-del $(OUTDIR)\vncviewer.exe
