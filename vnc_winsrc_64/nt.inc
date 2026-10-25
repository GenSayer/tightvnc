# nt.inc - settings shared by every nt.mak in this tree
#
# Hand-written for the NMAKE that ships with the Windows NT 3.5 era Visual C++
# releases and kept to the subset that all the later ones accept as well: the
# Alpha64 and Itanium Platform SDK compilers, Visual C++ 2005 and later for x64
# and Visual Studio 2019/2022 for ARM64 and ARM64EC.  Every nt.mak does
#
#     !INCLUDE <relative path>\nt.inc
#
# and is run from its own directory.  build.cmd at the top of the tree drives
# them in the right order.
#
# Command-line switches (all optional):
#
#     CPU=X64 | ARM64 | ARM64EC | AXP64 | IA64     supported targets
#     CPU=i386 | ALPHA | ARM                       32-bit targets, kept for
#                                                  testing only (unsupported)
#                                                  default: from
#                                                  PROCESSOR_ARCHITECTURE
#     DEBUG=1                         unoptimised build with /Zi + /debug
#     HTMLHELP=1                      link htmlhelp.lib (see below)
#
# Output goes to Rel_<CPU> or Dbg_<CPU> next to each nt.mak, so builds for
# different CPUs can live side by side.
#
# The compiler switches, defines and libraries are the ones the Developer
# Studio makefiles (WinVNC.mak, vncviewer.mak, WinVNC-axp64.mak,
# vncviewer-axp64.mak, WinVNC-ia64.mak, vncviewer-ia64.mak and friends) use,
# so the images match what that route builds.  Those makefiles are kept for
# the IDE route; this set is the one that builds every CPU from the command
# line.

!IF "$(CPU)" == ""
!IF "$(PROCESSOR_ARCHITECTURE)" == "ALPHA"
CPU=ALPHA
!ELSEIF "$(PROCESSOR_ARCHITECTURE)" == "ARM"
CPU=ARM
!ELSEIF "$(PROCESSOR_ARCHITECTURE)" == "AMD64"
CPU=X64
!ELSEIF "$(PROCESSOR_ARCHITECTURE)" == "ARM64"
CPU=ARM64
!ELSEIF "$(PROCESSOR_ARCHITECTURE)" == "IA64"
CPU=IA64
!ELSE
CPU=i386
!ENDIF
!ENDIF

# The compiler sets _M_AMD64 / _M_ARM64 / _M_IA64 / _M_IX86 / _M_ALPHA / _M_ARM
# and windows.h derives the _AMD64_ style macros from it, as in the Developer
# Studio makefiles.  The Alpha64 compiler is the exception and is given the
# defines its makefiles use.
#
#   MACHINE  /machine: value for the linker.
#   EHFLAG   exception handling switch.  /GX is the spelling of the old
#            compilers; Visual C++ 2005 and later still take it but warn, so
#            the CPUs that only ever see those use /EHsc (which VC6 knows too).
#   HHLIB    HtmlHelp import library; empty unless HTMLHELP=1 is given, see
#            below.
#   HHWARN   1 for the CPUs where linking HtmlHelp is known to be a problem;
#            the application nt.mak files print a warning then.
#   WINVER   value for the WinVNC WINVER define.
#   CEXTRA   extra switches and defines for every directory.
#   VEXTRA   extra defines for the vncviewer directory only.
#
# AXP64 /Ap64 selects the 64-bit Alpha code generator; /Gt0 turns off the
# small data threshold, as the vncviewer release makefile does (it is applied
# to every directory so all objects agree).  Warning level stays /W3.
!IF "$(CPU)" == "X64"
MACHINE=/machine:X64
EHFLAG=/EHsc
HHWARN=0
WINVER=0x0500
CEXTRA=
VEXTRA=/DWIN64 /D_WIN64 /D_WIN32_IE=0x600
!ELSEIF "$(CPU)" == "ARM64"
MACHINE=/machine:ARM64
EHFLAG=/EHsc
HHWARN=0
WINVER=0x0500
CEXTRA=
VEXTRA=/DWIN64 /D_WIN64 /D_WIN32_IE=0x600
!ELSEIF "$(CPU)" == "ARM64EC"
MACHINE=/machine:ARM64EC
EHFLAG=/EHsc
HHWARN=0
WINVER=0x0500
CEXTRA=/arm64EC
VEXTRA=/DWIN64 /D_WIN64 /D_WIN32_IE=0x600
!ELSEIF "$(CPU)" == "AXP64"
MACHINE=/machine:ALPHA64
EHFLAG=/GX
HHWARN=1
WINVER=0x0500
CEXTRA=/Ap64 /Gt0 /DWIN64 /D_WIN64 /DWp64 /DAp64 /D_AXP64_ /D_ALPHA64_ /DALPHA
VEXTRA=
!ELSEIF "$(CPU)" == "IA64"
MACHINE=/machine:IA64
EHFLAG=/GX
HHWARN=1
WINVER=0x0500
CEXTRA=
VEXTRA=/DWIN64 /D_WIN64 /D_WIN32_IE=0x600
!ELSEIF "$(CPU)" == "i386"
MACHINE=/machine:I386
EHFLAG=/GX
HHWARN=0
WINVER=0x0400
CEXTRA=
VEXTRA=
!ELSEIF "$(CPU)" == "ALPHA"
MACHINE=/machine:ALPHA
EHFLAG=/GX
HHWARN=0
WINVER=0x0400
CEXTRA=
VEXTRA=
!ELSEIF "$(CPU)" == "ARM"
MACHINE=/machine:ARM
EHFLAG=/EHsc
HHWARN=0
WINVER=0x0500
CEXTRA=/D_WIN32_IE=0x600 /D_ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE=1
VEXTRA=
!ELSE
!ERROR CPU must be X64, ARM64, ARM64EC, AXP64, IA64 (or, unsupported, i386, ALPHA, ARM); got "$(CPU)"
!ENDIF

# htmlhelp.lib.  The HtmlHelp() calls in both programs are commented out (any
# HtmlHelp call crashed the programs on IA64 before Windows Server 2003 SP1),
# so by default nothing is linked.  HTMLHELP=1 links $(HHLIB) for the CPU being
# built; put a matching htmlhelp.lib for that CPU in vncviewer\ and winvnc\
# (or on LIB) first: the one in the tree is for x64 only.  Expect a warning
# for IA64 and AXP64 builds.
!IF "$(HTMLHELP)" == "1"
HHLIB=htmlhelp.lib
!ELSE
HHLIB=
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

CBASE=/nologo /W3 $(EHFLAG) /DWIN32 /D_WINDOWS $(CEXTRA) $(CDBG)

# Plain /subsystem:windows: the linker's default subsystem version, exactly
# as the original makefiles.
LBASE=/nologo /subsystem:windows /incremental:no $(MACHINE) $(LDBG)

!IF "$(DEBUG)" == "1"
RCBASE=/r /d_DEBUG
!ELSE
RCBASE=/r /dNDEBUG
!ENDIF
