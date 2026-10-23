# libjpeg (winvnc) - Visual C++ 4.x NMAKE file.  Run from this directory;
# see ..\..\msvc4.inc.

!INCLUDE ..\..\msvc4.inc

CFLAGS=$(CBASE) $(CRTST) /D_MBCS /D_LIB

OBJS= \
	$(OUTDIR)\jcapimin.obj \
	$(OUTDIR)\jcapistd.obj \
	$(OUTDIR)\jccoefct.obj \
	$(OUTDIR)\jccolor.obj \
	$(OUTDIR)\jcdctmgr.obj \
	$(OUTDIR)\jchuff.obj \
	$(OUTDIR)\jcinit.obj \
	$(OUTDIR)\jcmainct.obj \
	$(OUTDIR)\jcmarker.obj \
	$(OUTDIR)\jcmaster.obj \
	$(OUTDIR)\jcomapi.obj \
	$(OUTDIR)\jcparam.obj \
	$(OUTDIR)\jcphuff.obj \
	$(OUTDIR)\jcprepct.obj \
	$(OUTDIR)\jcsample.obj \
	$(OUTDIR)\jctrans.obj \
	$(OUTDIR)\jdapimin.obj \
	$(OUTDIR)\jdapistd.obj \
	$(OUTDIR)\jdatadst.obj \
	$(OUTDIR)\jdatasrc.obj \
	$(OUTDIR)\jdcoefct.obj \
	$(OUTDIR)\jdcolor.obj \
	$(OUTDIR)\jddctmgr.obj \
	$(OUTDIR)\jdhuff.obj \
	$(OUTDIR)\jdinput.obj \
	$(OUTDIR)\jdmainct.obj \
	$(OUTDIR)\jdmarker.obj \
	$(OUTDIR)\jdmaster.obj \
	$(OUTDIR)\jdmerge.obj \
	$(OUTDIR)\jdphuff.obj \
	$(OUTDIR)\jdpostct.obj \
	$(OUTDIR)\jdsample.obj \
	$(OUTDIR)\jdtrans.obj \
	$(OUTDIR)\jerror.obj \
	$(OUTDIR)\jfdctflt.obj \
	$(OUTDIR)\jfdctfst.obj \
	$(OUTDIR)\jfdctint.obj \
	$(OUTDIR)\jidctflt.obj \
	$(OUTDIR)\jidctfst.obj \
	$(OUTDIR)\jidctint.obj \
	$(OUTDIR)\jidctred.obj \
	$(OUTDIR)\jmemmgr.obj \
	$(OUTDIR)\jmemnobs.obj \
	$(OUTDIR)\jquant1.obj \
	$(OUTDIR)\jquant2.obj \
	$(OUTDIR)\jutils.obj

all: $(OUTDIR) $(OUTDIR)\libjpeg.lib

$(OUTDIR):
	if not exist $(OUTDIR)\nul mkdir $(OUTDIR)

$(OUTDIR)\libjpeg.lib: $(OBJS)
	$(LIBEXE) /nologo /out:$@ $(OBJS)

.SUFFIXES: .c

{.}.c{$(OUTDIR)}.obj:
	$(CC) $(CFLAGS) /Fo$(OUTDIR)\ /c $<

clean:
	-del $(OUTDIR)\*.obj
	-del $(OUTDIR)\libjpeg.lib
