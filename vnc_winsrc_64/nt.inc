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
#     HELP=mini | html | none         context help system (see below);
#                                     default mini.  HTMLHELP=1 is the old
#                                     spelling of HELP=html
#     DLLCRT=1                        VNCHooks.dll uses the DLL CRT (/MD)
#                                     instead of the static one (see below)
#     COMCTL=4 | 5 | 6                compile for Common Controls 4.71, 5.80
#                                     or 6.0 (_WIN32_IE); 6 implies
#                                     VISUALSTYLES=1 (see below)
#     VISUALSTYLES=1                  link a manifest that selects Common
#                                     Controls 6 (see below)
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

# COMCTL=4 | 5 | 6 chooses the Common Controls level the programs are compiled
# for, by defining _WIN32_IE (0x0400 = comctl32 4.71 / IE 4, 0x0500 = 5.80 /
# IE 5, 0x0600 = 6.0 / XP).  That fixes the control structure sizes and flags
# the code uses; which comctl32.dll actually runs is still up to the target
# system.  COMCTL=6 also implies VISUALSTYLES=1 (below).  Without COMCTL the
# SDK's own default applies, except where this file sets 0x0600 for the CPU.
!IF "$(COMCTL)" == ""
IEDEF600=/D_WIN32_IE=0x0600
!ELSEIF "$(COMCTL)" == "4"
IEDEF600=
COMIE=0x0400
!ELSEIF "$(COMCTL)" == "5"
IEDEF600=
COMIE=0x0500
!ELSEIF "$(COMCTL)" == "6"
IEDEF600=
COMIE=0x0600
VISUALSTYLES=1
!ELSE
!ERROR COMCTL must be 4, 5 or 6 (got "$(COMCTL)")
!ENDIF

# The compiler sets _M_AMD64 / _M_ARM64 / _M_IA64 / _M_IX86 / _M_ALPHA / _M_ARM
# and windows.h derives the _AMD64_ style macros from it, as in the Developer
# Studio makefiles.  The Alpha64 compiler is the exception and is given the
# defines its makefiles use.
#
#   MACHINE  /machine: value for the linker.
#   LMACH    /machine: value for lib.exe; only ARM64EC needs one (lib refuses
#            the EC objects without it), the others take it from the objects.
#   EHFLAG   exception handling switch.  /GX is the spelling of the old
#            compilers; Visual C++ 2005 and later still take it but warn, so
#            the CPUs that only ever see those use /EHsc (which VC6 knows too).
#   HHWARN   1 for the CPUs where HtmlHelp is known to be a problem; the
#            application nt.mak files print a warning for HELP=html then.
#   WINVER   value for the WinVNC WINVER define.
#   CEXTRA   extra switches and defines for every directory.
#   VEXTRA   extra defines for the vncviewer directory only.
#
# AXP64 /Ap64 selects the 64-bit Alpha code generator; /Gt0 turns off the
# small data threshold, as the vncviewer release makefile does (it is applied
# to every directory so all objects agree).  Warning level stays /W3.
!IF "$(CPU)" == "X64"
MACHINE=/machine:X64
LMACH=
EHFLAG=/EHsc
HHWARN=0
WINVER=0x0500
CEXTRA=
VEXTRA=/DWIN64 /D_WIN64 $(IEDEF600)
!ELSEIF "$(CPU)" == "ARM64"
MACHINE=/machine:ARM64
LMACH=
EHFLAG=/EHsc
HHWARN=0
WINVER=0x0500
CEXTRA=
VEXTRA=/DWIN64 /D_WIN64 $(IEDEF600)
!ELSEIF "$(CPU)" == "ARM64EC"
MACHINE=/machine:ARM64EC
LMACH=/machine:ARM64EC
EHFLAG=/EHsc
HHWARN=0
WINVER=0x0500
CEXTRA=/arm64EC
VEXTRA=/DWIN64 /D_WIN64 $(IEDEF600)
!ELSEIF "$(CPU)" == "AXP64"
MACHINE=/machine:ALPHA64
LMACH=
EHFLAG=/GX
HHWARN=1
WINVER=0x0500
CEXTRA=/Ap64 /Gt0 /DWIN64 /D_WIN64 /DWp64 /DAp64 /D_AXP64_ /D_ALPHA64_ /DALPHA
VEXTRA=
!ELSEIF "$(CPU)" == "IA64"
MACHINE=/machine:IA64
LMACH=
EHFLAG=/GX
HHWARN=1
WINVER=0x0500
CEXTRA=
VEXTRA=/DWIN64 /D_WIN64 $(IEDEF600)
!ELSEIF "$(CPU)" == "i386"
MACHINE=/machine:I386
LMACH=
EHFLAG=/GX
HHWARN=0
WINVER=0x0400
CEXTRA=
VEXTRA=
!ELSEIF "$(CPU)" == "ALPHA"
MACHINE=/machine:ALPHA
LMACH=
EHFLAG=/GX
HHWARN=0
WINVER=0x0400
CEXTRA=
VEXTRA=
!ELSEIF "$(CPU)" == "ARM"
MACHINE=/machine:ARM
LMACH=
EHFLAG=/EHsc
HHWARN=0
WINVER=0x0500
CEXTRA=$(IEDEF600) /D_ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE=1
VEXTRA=
!ELSE
!ERROR CPU must be X64, ARM64, ARM64EC, AXP64, IA64 (or, unsupported, i386, ALPHA, ARM); got "$(CPU)"
!ENDIF

# HELP chooses the context help of the dialogs (F1 and the caption "?"
# button), by defining one of the VNCHELP_ macros of VNCHelp.h:
#
#   mini   VNCHELP_MINI: the built-in text popups of VNCHelp.cpp.  Nothing
#          extra is linked and hhctrl.ocx is not needed.  The default.
#   html   VNCHELP_HTML: the original HtmlHelp popups.  Links htmlhelp.lib;
#          put a matching htmlhelp.lib for the CPU in vncviewer\ and winvnc\
#          (or on LIB) first: the one in the tree is for x64 only.  HtmlHelp
#          calls crashed the programs on IA64 before Windows Server 2003 SP1,
#          so expect a warning for IA64 and AXP64 builds.
#   none   VNCHELP_NONE: no context help; the "?" buttons are removed.
#
# HHLIB is the HtmlHelp import library, empty unless HELP=html.  HELPSEL holds
# the choice, because a makefile cannot override a command-line HELP=.
!IF "$(HELP)" != ""
HELPSEL=$(HELP)
!ELSEIF "$(HTMLHELP)" == "1"
HELPSEL=html
!ELSE
HELPSEL=mini
!ENDIF
!IF "$(HELPSEL)" == "mini"
HELPDEF=/DVNCHELP_MINI
HHLIB=
!ELSEIF "$(HELPSEL)" == "html"
HELPDEF=/DVNCHELP_HTML
HHLIB=htmlhelp.lib
!ELSEIF "$(HELPSEL)" == "none"
HELPDEF=/DVNCHELP_NONE
HHLIB=
!ELSE
!ERROR HELP must be mini, html or none (got "$(HELP)")
!ENDIF

# CRTST is the static multithreaded CRT the programs and libraries use.  CRTDLL
# is what VNCHooks.dll uses.  The original makefiles use the DLL CRT (/MD) there,
# but that makes the DLL import msvcr80.dll (Visual C++ 2005 and later: the
# matching redistributable), so WinVNC fails to start on a machine without it.
# The default here is therefore the static CRT as well, so no CRT DLL has to be
# installed.  DLLCRT=1 selects the DLL CRT for VNCHooks.dll only (the original
# behaviour; the CRT DLL of the compiler must then be present on the target).
# Applied to every directory (the per-CPU values above only carry the 0x0600
# default that COMCTL replaces).
!IF "$(COMCTL)" != ""
CEXTRA=$(CEXTRA) /D_WIN32_IE=$(COMIE)
!ENDIF

!IF "$(DEBUG)" == "1"
CDBG=/Zi /Od /D_DEBUG
LDBG=/debug
CRTST=/MTd
CRTDLL=/MTd
OUTDIR=Dbg_$(CPU)
!ELSE
CDBG=/O2 /DNDEBUG
LDBG=
CRTST=/MT
CRTDLL=/MT
OUTDIR=Rel_$(CPU)
!ENDIF

!IF "$(DLLCRT)" == "1"
!IF "$(DEBUG)" == "1"
CRTDLL=/MDd
!ELSE
CRTDLL=/MD
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

# VISUALSTYLES=1 adds visualstyles.res (the manifest in visualstyles.manifest,
# compiled by visualstyles.rc) to WinVNC.exe and vncviewer.exe.  The manifest
# asks for Microsoft.Windows.Common-Controls 6.0, so on Windows XP and later
# the buttons, tabs, trackbars and so on take the current theme instead of the
# flat Windows 95 look.  Older systems ignore it.  The manifest is added as a
# resource, so it works with every resource compiler and linker used here and
# no manifest tool is needed.  Without the switch nothing changes.
!IF "$(VISUALSTYLES)" == "1"
VSRES=$(OUTDIR)\visualstyles.res
!ELSE
VSRES=
!ENDIF
