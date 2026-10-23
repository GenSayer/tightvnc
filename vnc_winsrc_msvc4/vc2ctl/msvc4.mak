# vc2ctl - built-in common controls (see vc2ctl.h).  Visual C++ 4.x NMAKE
# file.  Run from this directory; see ..\msvc4.inc.  Linked by vncviewer and
# WinVNC, which use it on Windows NT before 3.51 only.
#
# vc2commctl.h and vc2ctlstyles.h are copies of the ones in the VC++ 2.x tree
# (vnc_winsrc_vc2\vncviewer).  With the MSVC 4.x commctrl.h every block in
# them is skipped, because that header already declares everything.
#
# vc2.mak in this directory is the VC++ 2.x tree's makefile, copied with the
# sources.  It is not used here.

!INCLUDE ..\msvc4.inc

CFLAGS=$(CBASE) $(CRTST) /D_LIB

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

$(OBJS): vc2ctl.h vc2ctli.h vc2commctl.h vc2ctlstyles.h

.SUFFIXES: .cpp

{.}.cpp{$(OUTDIR)}.obj:
	$(CC) $(CFLAGS) /Fo$(OUTDIR)\ /c $<

clean:
	-del $(OUTDIR)\*.obj
	-del $(OUTDIR)\vc2ctl.lib
