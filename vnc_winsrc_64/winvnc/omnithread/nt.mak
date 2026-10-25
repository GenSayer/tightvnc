# omnithread (winvnc) - NMAKE file.  Run from this directory;
# see ..\..\nt.inc.

!INCLUDE ..\..\nt.inc

CFLAGS=$(CBASE) $(CRTST) /D__NT__ /D_WINSTATIC /D__WIN32__

OBJS=$(OUTDIR)\nt.obj

all: $(OUTDIR) $(OUTDIR)\omnithread.lib

$(OUTDIR):
	if not exist $(OUTDIR)\nul mkdir $(OUTDIR)

$(OUTDIR)\omnithread.lib: $(OBJS)
	$(LIBEXE) /nologo $(LMACH) /out:$@ $(OBJS)

.SUFFIXES: .cpp

{.}.cpp{$(OUTDIR)}.obj:
	$(CC) $(CFLAGS) /Fo$(OUTDIR)\ /c $<

clean:
	-del $(OUTDIR)\*.obj
	-del $(OUTDIR)\omnithread.lib
