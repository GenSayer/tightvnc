# nt.inc - settings shared by every nt.mak in this tree
#
# Hand-written for the NMAKE that ships with Visual C++ 4.x - 6.0 (i386 and
# the Alpha edition) and Visual Studio 2015 (ARM cross tools), and kept to
# the subset that every one of them accepts.  Every nt.mak does
#
#     !INCLUDE <relative path>\nt.inc
#
# and is run from its own directory.  build.cmd at the top of the tree drives
# them in the right order.
#
# Command-line switches (all optional):
#
#     CPU=i386 | ALPHA | ARM          target CPU; default from
#                                     PROCESSOR_ARCHITECTURE
#     DEBUG=1                         unoptimised build with /Zi + /debug
#     STATICCRT=1                     VNCHooks.dll uses the static CRT (/MT)
#                                     instead of the DLL CRT (see below)
#
# Output goes to Rel_<CPU> or Dbg_<CPU> next to each nt.mak, so builds for
# different CPUs can live side by side.
#
# The compiler switches, defines and libraries are the ones the Developer
# Studio makefiles (WinVNC.mak, vncviewer.mak and friends) use, so the images
# match what that route builds.  Those makefiles are kept for the IDE route;
# this set is the one that builds every CPU from the command line.

!IF "$(CPU)" == ""
!IF "$(PROCESSOR_ARCHITECTURE)" == "ALPHA"
CPU=ALPHA
!ELSEIF "$(PROCESSOR_ARCHITECTURE)" == "ARM"
CPU=ARM
!ELSE
CPU=i386
!ENDIF
!ENDIF

# No CPU define is passed: the compiler sets _M_IX86 / _M_ALPHA / _M_ARM and
# windows.h derives _X86_ / _ALPHA_ / _ARM_ from it, as in the Developer
# Studio makefiles.
#
#   EHFLAG   exception handling switch.  /GX is the spelling of Visual C++ 4.x
#            to 6.0; Visual Studio 2015 still takes it but warns, so ARM uses
#            /EHsc (which VC6 also knows).
#   HHLIB    HtmlHelp import library, linked for every CPU.  The HtmlHelp.lib
#            in the source tree is for x86; put the Alpha or ARM version in
#            the tree or on LIB before building for those CPUs.
#   WINVER   value for the WinVNC WINVER define.
#   CEXTRA   extra defines for every directory.  On ARM,
#            _ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE=1 switches off the
#            "Compiling Desktop applications for the ARM platform is not
#            supported" error (C1189) of the Windows SDK headers, so desktop
#            programs build for Windows RT.
!IF "$(CPU)" == "i386"
MACHINE=/machine:I386
EHFLAG=/GX
HHLIB=htmlhelp.lib
WINVER=0x0400
CEXTRA=
!ELSEIF "$(CPU)" == "ALPHA"
MACHINE=/machine:ALPHA
EHFLAG=/GX
HHLIB=htmlhelp.lib
WINVER=0x0400
CEXTRA=
!ELSEIF "$(CPU)" == "ARM"
MACHINE=/machine:ARM
EHFLAG=/EHsc
HHLIB=htmlhelp.lib
WINVER=0x0500
CEXTRA=/D_WIN32_IE=0x600 /D_ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE=1
!ELSE
!ERROR CPU must be i386, ALPHA or ARM (got "$(CPU)")
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

# VNCHooks.dll uses the DLL CRT by default, as the original makefile does.
# STATICCRT=1 links the static CRT into it instead, so that no CRT DLL has to
# be installed (with Visual C++ 2005 and later /MD makes the DLL import
# msvcr80.dll or its successor, which must then be present where WinVNC runs).
!IF "$(STATICCRT)" == "1"
!IF "$(DEBUG)" == "1"
CRTDLL=/MTd
!ELSE
CRTDLL=/MT
!ENDIF
!ENDIF

CC=cl
LINK=link
LIBEXE=lib
RC=rc

CBASE=/nologo /W3 $(EHFLAG) /DWIN32 /D_WINDOWS $(CEXTRA) $(CDBG)

# Plain /subsystem:windows: the linker's default subsystem version, exactly
# as the original makefiles.
LBASE=/nologo /subsystem:windows /incremental:no $(MACHINE) $(LDBG)

!IF "$(DEBUG)" == "1"
RCBASE=/r /d_DEBUG
!ELSE
RCBASE=/r /dNDEBUG
!ENDIF
