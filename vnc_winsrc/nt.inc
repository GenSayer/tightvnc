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
#     COMCTL=4 | 5 | 6                compile for Common Controls 4.71, 5.80
#                                     or 6.0 (_WIN32_IE); 6 implies
#                                     VISUALSTYLES=1 (see below)
#     VISUALSTYLES=1                  link a manifest that selects Common
#                                     Controls 6 (see below)
#     HELP=mini | html | none         context help system (see below);
#                                     default mini
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

# No CPU define is passed: the compiler sets _M_IX86 / _M_ALPHA / _M_ARM and
# windows.h derives _X86_ / _ALPHA_ / _ARM_ from it, as in the Developer
# Studio makefiles.
#
#   EHFLAG   exception handling switch.  /GX is the spelling of Visual C++ 4.x
#            to 6.0; Visual Studio 2015 still takes it but warns, so ARM uses
#            /EHsc (which VC6 also knows).
#   WINVER   value for the WinVNC WINVER define.
#   CEXTRA   extra defines for every directory.  On ARM,
#            _ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE=1 switches off the
#            "Compiling Desktop applications for the ARM platform is not
#            supported" error (C1189) of the Windows SDK headers, so desktop
#            programs build for Windows RT.
!IF "$(CPU)" == "i386"
MACHINE=/machine:I386
EHFLAG=/GX
WINVER=0x0400
CEXTRA=
!ELSEIF "$(CPU)" == "ALPHA"
MACHINE=/machine:ALPHA
EHFLAG=/GX
WINVER=0x0400
CEXTRA=
!ELSEIF "$(CPU)" == "ARM"
MACHINE=/machine:ARM
EHFLAG=/EHsc
WINVER=0x0500
CEXTRA=$(IEDEF600) /D_ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE=1
!ELSE
!ERROR CPU must be i386, ALPHA or ARM (got "$(CPU)")
!ENDIF

# HELP chooses the context help of the dialogs (F1 and the caption "?"
# button), by defining one of the VNCHELP_ macros of VNCHelp.h:
#
#   mini   VNCHELP_MINI: the built-in text popups of VNCHelp.cpp.  Nothing
#          extra is linked and hhctrl.ocx is not needed.  The default.
#   html   VNCHELP_HTML: the original HtmlHelp popups.  Links htmlhelp.lib;
#          the HtmlHelp.lib in the source tree is for x86, so put the Alpha
#          or ARM version in the tree or on LIB before building for those
#          CPUs.  The target needs HTML Help (hhctrl.ocx) installed.
#   none   VNCHELP_NONE: no context help; the "?" buttons are removed.
#
# HHLIB is the HtmlHelp import library, empty unless HELP=html.
# (HELPSEL, because a makefile cannot override a command-line HELP=.)
!IF "$(HELP)" == ""
HELPSEL=mini
!ELSE
HELPSEL=$(HELP)
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

# CRTST is the static multithreaded CRT the programs and libraries use; CRTDLL
# is the DLL CRT that VNCHooks.dll uses.  Both as in the original makefiles.
# Applied to every directory (the per-CPU values above only carry the 0x0600
# default that COMCTL replaces).
!IF "$(COMCTL)" != ""
CEXTRA=$(CEXTRA) /D_WIN32_IE=$(COMIE)
!ENDIF

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
