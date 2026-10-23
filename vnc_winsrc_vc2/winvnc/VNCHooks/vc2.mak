# VNCHooks.dll - VC++ 2.x NMAKE file.  Run from this directory; see
# ..\..\vc2.inc.
#
# NT-only at run time: WinVNC LoadLibrary()s this DLL on Windows NT and never
# touches it on Win32s (see ..\VNCHooksStub.cpp).  Copy VNCHooks.dll next to
# WinVNC.exe; build.cmd does this.  No /FI here - this DLL does not use the
# WinVNC headers.

!INCLUDE ..\..\vc2.inc

CFLAGS=$(CBASE)

# gdi32 for the update-region code (CreateRectRgn, GetRegionData,
# DeleteObject) - all core Win32, present on NT 3.1.
LIBS=kernel32.lib user32.lib gdi32.lib advapi32.lib

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
	$(RC) $(RCBASE) /fo$@ VNCHooks.rc

.SUFFIXES: .cpp

{.}.cpp{$(OUTDIR)}.obj:
	$(CC) $(CFLAGS) /Fo$(OUTDIR)\ /c $<

clean:
	-del $(OUTDIR)\*.obj
	-del $(OUTDIR)\*.res
	-del $(OUTDIR)\VNCHooks.*
