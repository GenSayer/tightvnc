# zlib (winvnc) - NMAKE file.  Run from this directory; see
# ..\..\nt.inc.

!INCLUDE ..\..\nt.inc

CFLAGS=$(CBASE) $(CRTST) /D_MBCS /D_LIB /D__NT__ /D_WINSTATIC /D__WIN32__

OBJS= \
	$(OUTDIR)\adler32.obj \
	$(OUTDIR)\compress.obj \
	$(OUTDIR)\crc32.obj \
	$(OUTDIR)\deflate.obj \
	$(OUTDIR)\infblock.obj \
	$(OUTDIR)\infcodes.obj \
	$(OUTDIR)\inffast.obj \
	$(OUTDIR)\inflate.obj \
	$(OUTDIR)\inftrees.obj \
	$(OUTDIR)\infutil.obj \
	$(OUTDIR)\maketree.obj \
	$(OUTDIR)\trees.obj \
	$(OUTDIR)\uncompr.obj \
	$(OUTDIR)\zutil.obj

all: $(OUTDIR) $(OUTDIR)\zlib.lib

$(OUTDIR):
	if not exist $(OUTDIR)\nul mkdir $(OUTDIR)

$(OUTDIR)\zlib.lib: $(OBJS)
	$(LIBEXE) /nologo $(LMACH) /out:$@ $(OBJS)

.SUFFIXES: .c

{.}.c{$(OUTDIR)}.obj:
	$(CC) $(CFLAGS) /Fo$(OUTDIR)\ /c $<

clean:
	-del $(OUTDIR)\*.obj
	-del $(OUTDIR)\zlib.lib
