# vc2ctl - built-in common controls (see vc2ctl.h).  VC++ 2.x NMAKE file.
# Run from this directory; see ..\vc2.inc.  Linked by vncviewer and WinVNC.
#
# vc2commctl.h comes from ..\vncviewer (an identical copy is in ..\winvnc).
# win32s_fix.h is NOT force-included: this library uses only Win32 types.

!INCLUDE ..\vc2.inc

CFLAGS=$(CBASE) /I ..\vncviewer /D_LIB

OBJS= \
	$(OUTDIR)\vc2ctl.obj \
	$(OUTDIR)\ctldraw.obj \
	$(OUTDIR)\tab.obj \
	$(OUTDIR)\trackbar.obj \
	$(OUTDIR)\updown.obj \
	$(OUTDIR)\progress.obj \
	$(OUTDIR)\listview.obj \
	$(OUTDIR)\treeview.obj \
	$(OUTDIR)\toolbar.obj \
	$(OUTDIR)\tooltip.obj \
	$(OUTDIR)\pagedlg.obj

all: $(OUTDIR) $(OUTDIR)\vc2ctl.lib

$(OUTDIR):
	if not exist $(OUTDIR)\nul mkdir $(OUTDIR)

$(OUTDIR)\vc2ctl.lib: $(OBJS)
	$(LIBEXE) /nologo /out:$@ $(OBJS)

$(OBJS): vc2ctl.h vc2ctli.h ..\vncviewer\vc2commctl.h ..\vncviewer\vc2ctlstyles.h

.SUFFIXES: .cpp

{.}.cpp{$(OUTDIR)}.obj:
	$(CC) $(CFLAGS) /Fo$(OUTDIR)\ /c $<

clean:
	-del $(OUTDIR)\*.obj
	-del $(OUTDIR)\vc2ctl.lib
