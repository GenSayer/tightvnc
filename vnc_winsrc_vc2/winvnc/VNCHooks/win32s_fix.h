// VNCHooks/win32s_fix.h
//
// The VNCHooks DLL is NOT BUILT for the Win32s port - see the long note at the
// top of VNCHooks/VNCHooks.cpp.  System-wide Windows hooks installed from a
// Win32s DLL cannot reach 16-bit tasks, and on Windows 3.1 essentially every
// application is 16-bit, so the hook-based update notification cannot work.
// The server runs in full-screen polling mode instead.
//
// This file is kept only so that the VNCHooks project still configures and
// compiles if someone builds it for a Win32 target.  It defers to the single
// real copy so that bool/true/false cannot drift (see ..\win32s_fix.h).

#ifndef WIN32S_FIX_VNCHOOKS_SHIM_H
#define WIN32S_FIX_VNCHOOKS_SHIM_H

#include "..\win32s_fix.h"

#endif
