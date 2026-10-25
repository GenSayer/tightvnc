# VNCHooks.dll - NMAKE file.  Run from this directory; see
# ..\..\nt.inc.
#
# WinVNC links VNCHooks.lib (the import library made here), so build this
# before ..\nt.mak and ship VNCHooks.dll next to WinVNC.exe; build.cmd
# copies it there.  Uses the DLL CRT (/MD), as the original makefile does.

!INCLUDE ..\..\nt.inc

CFLAGS=$(CBASE) $(CRTDLL) /D_WIN32_WINNT=0x0400

LIBS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib

OBJS=$(OUTDIR)\VNCHooks.obj

all: $(OUTDIR) $(OUTDIR)\VNCHooks.dll

$(OUTDIR):
	if not exist $(OUTDIR)\nul mkdir $(OUTDIR)

$(OUTDIR)\VNCHooks.dll: $(OBJS) $(OUTDIR)\VNCHooks.res VNCHooks.def
	$(LINK) @<<
$(LBASE) /dll /def:VNCHooks.def /implib:$(OUTDIR)\VNCHooks.lib /out:$@
$(OBJS)
$(OUTDIR)\VNCHooks.res
$(LIBS)
<<

$(OUTDIR)\VNCHooks.res: VNCHooks.rc resource.h
	$(RC) $(RCBASE) /l 0x809 /fo$@ VNCHooks.rc

.SUFFIXES: .cpp

{.}.cpp{$(OUTDIR)}.obj:
	$(CC) $(CFLAGS) /Fo$(OUTDIR)\ /c $<

clean:
	-del $(OUTDIR)\*.obj
	-del $(OUTDIR)\*.res
	-del $(OUTDIR)\VNCHooks.*
