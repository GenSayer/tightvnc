# msvc4.inc - settings shared by every msvc4.mak in this tree
#
# Hand-written for the NMAKE that ships with Visual C++ 4.x (x86 and the
# MIPS, Alpha and PowerPC RISC editions).  Every msvc4.mak does
#
#     !INCLUDE <relative path>\msvc4.inc
#
# and is run from its own directory.  build.cmd at the top of the tree drives
# them in the right order.
#
# Command-line switches (all optional):
#
#     CPU=i386 | MIPS | ALPHA | PPC   target CPU; default from
#                                     PROCESSOR_ARCHITECTURE
#     DEBUG=1                         unoptimised build with /Zi + /debug
#
# Output goes to Rel_<CPU> or Dbg_<CPU> next to each msvc4.mak, so builds for
# different CPUs can live side by side.
#
# The compiler switches, defines and libraries are the ones the Developer
# Studio makefiles (WinVNC.mak, vncviewer.mak and friends) use, so the images
# match what that route builds.  Those makefiles are kept for the IDE / VC6
# route; this set is the one that builds every CPU.

!IF "$(CPU)" == ""
!IF "$(PROCESSOR_ARCHITECTURE)" == "MIPS"
CPU=MIPS
!ELSEIF "$(PROCESSOR_ARCHITECTURE)" == "ALPHA"
CPU=ALPHA
!ELSEIF "$(PROCESSOR_ARCHITECTURE)" == "PPC"
CPU=PPC
!ELSE
CPU=i386
!ENDIF
!ENDIF

# No CPU define is passed: the compiler sets _M_IX86 / _M_MRX000 / _M_ALPHA /
# _M_PPC and windows.h derives _X86_ / _MIPS_ / _ALPHA_ / _PPC_ from it, as in
# the Developer Studio makefiles.
!IF "$(CPU)" == "i386"
MACHINE=/machine:I386
!ELSEIF "$(CPU)" == "MIPS"
MACHINE=/machine:MIPS
!ELSEIF "$(CPU)" == "ALPHA"
MACHINE=/machine:ALPHA
!ELSEIF "$(CPU)" == "PPC"
MACHINE=/machine:PPC
!ELSE
!ERROR CPU must be i386, MIPS, ALPHA or PPC (got "$(CPU)")
!ENDIF

# CRTST is the static multithreaded CRT the programs and libraries use; CRTDLL
# is the DLL CRT that VNCHooks.dll uses.  Both as in the original makefiles.
!IF "$(DEBUG)" == "1"
CDBG=/Zi /Od /D_DEBUG
LDBG=/debug
CRTST=/MTd
CRTDLL=/MDd
OUTDIR=Dbg_$(CPU)
!ELSE
CDBG=/O2 /DNDEBUG
LDBG=
CRTST=/MT
CRTDLL=/MD
OUTDIR=Rel_$(CPU)
!ENDIF

CC=cl
LINK=link
LIBEXE=lib
RC=rc

CBASE=/nologo /W3 /GX /DWIN32 /D_WINDOWS $(CDBG)

# Plain /subsystem:windows: the linker's default subsystem version, exactly
# as the original makefiles.  Do not add a version here - it changes how the
# dialogs look on Windows 95 and NT 4.
LBASE=/nologo /subsystem:windows /incremental:no $(MACHINE) $(LDBG)

!IF "$(DEBUG)" == "1"
RCBASE=/r /d_DEBUG
!ELSE
RCBASE=/r /dNDEBUG
!ENDIF
