# vc2.inc - settings shared by every vc2.mak in this tree
#
# Hand-written for the NMAKE that ships with Visual C++ 2.x (and kept to the
# subset that later NMAKEs also accept).  Every vc2.mak does
#
#     !INCLUDE <relative path>\vc2.inc
#
# and is run from its own directory.  build.cmd at the top of the tree drives
# them in the right order.
#
# Command-line switches (all optional):
#
#     CPU=i386 | MIPS | ALPHA   target CPU; default from PROCESSOR_ARCHITECTURE
#     DEBUG=1                   unoptimised build with /Zi + /debug
#     FORCEINC=                 set empty if your cl has no /FI switch (see
#                               BUILDING.txt)
#     HELP=mini | none          context help system (see below);
#                               default mini
#
# Output goes to Rel_<CPU> or Dbg_<CPU> next to each vc2.mak, so builds for
# different CPUs can live side by side.

!IF "$(CPU)" == ""
!IF "$(PROCESSOR_ARCHITECTURE)" == "MIPS"
CPU=MIPS
!ELSEIF "$(PROCESSOR_ARCHITECTURE)" == "ALPHA"
CPU=ALPHA
!ELSE
CPU=i386
!ENDIF
!ENDIF

!IF "$(CPU)" == "i386"
CPUDEF=/D_X86_=1
MACHINE=/machine:I386
!ELSEIF "$(CPU)" == "MIPS"
CPUDEF=/D_MIPS_=1
MACHINE=/machine:MIPS
!ELSEIF "$(CPU)" == "ALPHA"
CPUDEF=/D_ALPHA_=1
MACHINE=/machine:ALPHA
!ELSE
!ERROR CPU must be i386, MIPS or ALPHA (got "$(CPU)")
!ENDIF

# VC++ 2.x has no debug CRT (libcd.lib / crtdbg.h arrived with 4.0), so both
# configurations link the static single-threaded libc.lib (/ML).  That is also
# the only CRT that is safe under Win32s.
!IF "$(DEBUG)" == "1"
CDBG=/Zi /Od /D_DEBUG
LDBG=/debug
OUTDIR=Dbg_$(CPU)
!ELSE
CDBG=/O2 /DNDEBUG
LDBG=
OUTDIR=Rel_$(CPU)
!ENDIF

!IFNDEF FORCEINC
FORCEINC=/FIwin32s_fix.h
!ENDIF

# HELP chooses the context help of the dialogs (F1 and the caption "?"
# button), by defining one of the VNCHELP_ macros of VNCHelp.h:
#
#   mini   VNCHELP_MINI: the built-in text popups of VNCHelp.cpp, with F1 and
#          a "What's This?" system menu command on NT 3.x and Win32s.  The
#          default.
#   none   VNCHELP_NONE: no context help; the "?" buttons are removed.
#
# The HtmlHelp popups of vnc_winsrc are not offered: an import of
# htmlhelp.lib would stop the programs loading on NT 3.x and Win32s.  HELPSEL
# holds the choice, because a makefile cannot override a command-line HELP=.
!IF "$(HELP)" == ""
HELPSEL=mini
!ELSE
HELPSEL=$(HELP)
!ENDIF
!IF "$(HELPSEL)" == "mini"
HELPDEF=/DVNCHELP_MINI
!ELSEIF "$(HELPSEL)" == "none"
HELPDEF=/DVNCHELP_NONE
!ELSE
!ERROR HELP must be mini or none (got "$(HELP)")
!ENDIF

CC=cl
LINK=link
LIBEXE=lib
RC=rc

CBASE=/nologo /ML /W3 /GX $(CPUDEF) /DWIN32 /D_WINDOWS $(CDBG)

# /subsystem:windows,3.10 is what lets NT 3.1 and Win32s load the images.
LBASE=/nologo /subsystem:windows,3.10 /incremental:no $(MACHINE) $(LDBG)

RCBASE=/r
!IF "$(DEBUG)" == "1"
RCBASE=$(RCBASE) /d_DEBUG
!ELSE
RCBASE=$(RCBASE) /dNDEBUG
!ENDIF
