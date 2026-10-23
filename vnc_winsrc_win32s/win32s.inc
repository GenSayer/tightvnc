# win32s.inc - settings shared by every win32s.mak in this tree
#
# Hand-written for the NMAKE that ships with Visual C++ (and kept to the
# subset that later NMAKEs also accept).  Every win32s.mak does
#
#     !INCLUDE <relative path>\win32s.inc
#
# and is run from its own directory.  build.cmd at the top of the tree drives
# them in the right order.
#
# Command-line switches (all optional):
#
#     DEBUG=1                   unoptimised build with /Zi + /debug
#     FORCEINC=                 set empty if your cl has no /FI switch
#
# Output goes to Rel_i386 or Dbg_i386 next to each win32s.mak.

# Win32s is x86-only; the other CPUs are built from vnc_winsrc_msvc4.
CPU=i386
CPUDEF=/D_X86_=1
MACHINE=/machine:I386

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
