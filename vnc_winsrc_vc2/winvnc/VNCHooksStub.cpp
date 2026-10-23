//  VNCHooksStub.cpp - run-time loader for VNCHooks.dll, with Win32s stubs
//
//  This file is part of the TightVNC VC++ 2.x / NT 3.1 / Win32s port.
//
//  The VNC system is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE DOES
//
// WinVNC.exe has to load on Win32s, where the hook DLL cannot work, and on
// Windows NT (3.1 included), where it can.  So WinVNC never imports
// VNCHooks.lib.  Instead this file defines the eight VNCHooks entry points
// itself and:
//
//   * on NT, LoadLibrary()s VNCHooks.dll (from next to WinVNC.exe) the first
//     time a hook is requested and forwards through GetProcAddress;
//
//   * on Win32s - or on NT when the DLL or an export is missing - behaves
//     exactly like the old Win32s stubs: SetHook() returns FALSE, which
//     vncDesktop::ActivateHooks handles by logging it and switching on
//     full-screen polling.  That is the normal operating mode on Win32s.
//
// WHY THE DLL CANNOT WORK ON WIN32S (see also VNCHooks/VNCHooks.h)
//
//   VNCHooks.dll installs system-wide hooks (WH_CALLWNDPROC, WH_GETMESSAGE,
//   WH_SYSMSGFILTER with a thread ID of 0) so that every application notifies
//   the server when it repaints.  A global hook works by injecting the DLL into
//   each hooked process.  Under Win32s all Win32 apps share one VM and the
//   Windows 3.1 16-bit message queue, and a 32-bit DLL cannot be injected into
//   a 16-bit task - which is what every Windows 3.1 application is.  There is
//   no dynamic-import workaround for that, so the DLL is never even loaded
//   there.
//
// WHAT IS FORWARDED
//
//   SetHook / UnSetHook                - screen-change notification.
//   SetKeyboardFilterHook /
//   SetMouseFilterHook                 - "disable local input while a client is
//                                        connected"; the DLL does all the work.
//
//   The four PRIORITY hooks ("local input takes priority") stay stubbed even
//   on NT.  They post RFB_LOCAL_KEYBOARD / RFB_LOCAL_MOUSE to the server, and
//   the Win32s tree dropped that handler from vncDesktop (it needed 64-bit
//   FILETIME arithmetic), so installing them would only add overhead.  The LL
//   variants need WH_KEYBOARD_LL / WH_MOUSE_LL (NT 4.0 SP3+) in any case.
//
// The DLL is never freed: like SHELL32/COMCTL32 in Win32sApi.cpp it is used for
// the life of the process, and unloading a DLL whose hooks may still be
// mapped into other processes buys nothing.
// ---------------------------------------------------------------------------

#include "stdhdrs.h"
#include "VNCHooks\VNCHooks.h"
#include "Win32sApi.h"

typedef BOOL (*PFNSETHOOK)(HWND, UINT, UINT, UINT);
typedef BOOL (*PFNUNSETHOOK)(HWND);
typedef BOOL (*PFNSETFILTERHOOK)(BOOL);

static int              g_hooksTried = 0;
static HINSTANCE        g_hHooks = NULL;
static PFNSETHOOK       g_pSetHook = NULL;
static PFNUNSETHOOK     g_pUnSetHook = NULL;
static PFNSETFILTERHOOK g_pSetKeyboardFilterHook = NULL;
static PFNSETFILTERHOOK g_pSetMouseFilterHook = NULL;

// Load VNCHooks.dll once, on NT only.  Returns TRUE if SetHook/UnSetHook are
// available.
static BOOL
LoadHooks()
{
	if (!g_hooksTried) {
		g_hooksTried = 1;

		if (Win32sIsWin32s()) {
			vnclog.Print(LL_INTINFO,
				VNCLOG("VNCHooks not used on Win32s - using polling\n"));
			return FALSE;
		}

		// Do not let a missing DLL produce a system error box.
		UINT savedErrorMode = SetErrorMode(SEM_NOOPENFILEERRORBOX |
										   SEM_FAILCRITICALERRORS);
		g_hHooks = LoadLibrary("VNCHooks.dll");
		SetErrorMode(savedErrorMode);

		if (g_hHooks == NULL) {
			vnclog.Print(LL_INTWARN,
				VNCLOG("VNCHooks.dll not loaded (error %d) - using polling\n"),
				GetLastError());
			return FALSE;
		}

		g_pSetHook = (PFNSETHOOK)GetProcAddress(g_hHooks, "SetHook");
		g_pUnSetHook = (PFNUNSETHOOK)GetProcAddress(g_hHooks, "UnSetHook");
		g_pSetKeyboardFilterHook =
			(PFNSETFILTERHOOK)GetProcAddress(g_hHooks, "SetKeyboardFilterHook");
		g_pSetMouseFilterHook =
			(PFNSETFILTERHOOK)GetProcAddress(g_hHooks, "SetMouseFilterHook");

		if (g_pSetHook == NULL || g_pUnSetHook == NULL) {
			vnclog.Print(LL_INTERR,
				VNCLOG("VNCHooks.dll lacks SetHook/UnSetHook - using polling\n"));
			g_pSetHook = NULL;
			g_pUnSetHook = NULL;
		} else {
			vnclog.Print(LL_INTINFO, VNCLOG("VNCHooks.dll loaded\n"));
		}
	}

	return (g_pSetHook != NULL);
}

extern "C" {

BOOL SetHook(HWND hWnd, UINT UpdateMsg, UINT CopyMsg, UINT MouseMsg)
{
	if (!LoadHooks())
		return FALSE;
	return g_pSetHook(hWnd, UpdateMsg, CopyMsg, MouseMsg);
}

BOOL UnSetHook(HWND hWnd)
{
	// Nothing was hooked if the DLL never loaded - "removed" is the truth.
	if (!LoadHooks())
		return TRUE;
	return g_pUnSetHook(hWnd);
}

BOOL SetKeyboardFilterHook(BOOL activate)
{
	if (!LoadHooks() || g_pSetKeyboardFilterHook == NULL)
		return FALSE;
	return g_pSetKeyboardFilterHook(activate);
}

BOOL SetMouseFilterHook(BOOL activate)
{
	if (!LoadHooks() || g_pSetMouseFilterHook == NULL)
		return FALSE;
	return g_pSetMouseFilterHook(activate);
}

// Priority hooks: deliberately not forwarded - see the note at the top.

BOOL SetKeyboardPriorityHook(HWND hwnd, BOOL activate, UINT LocalMsg)
{
	return FALSE;
}

BOOL SetMousePriorityHook(HWND hwnd, BOOL activate, UINT LocalMsg)
{
	return FALSE;
}

BOOL SetKeyboardPriorityLLHook(HWND hwnd, BOOL activate, UINT LocalMsg)
{
	return FALSE;
}

BOOL SetMousePriorityLLHook(HWND hwnd, BOOL activate, UINT LocalMsg)
{
	return FALSE;
}

} // extern "C"
