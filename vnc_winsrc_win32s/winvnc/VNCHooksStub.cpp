//  VNCHooksStub.cpp - VNCHooks replacement for the Win32s / Windows 3.1 port
//
//  This file is part of the TightVNC Win32s port.
//
//  The VNC system is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
// ---------------------------------------------------------------------------
// WHY THE HOOK DLL IS REPLACED BY STUBS
//
// See the long note at the top of VNCHooks/VNCHooks.h.  In short:
//
//   VNCHooks.dll works by installing system-wide Windows hooks
//   (WH_CALLWNDPROC, WH_GETMESSAGE, WH_SYSMSGFILTER with a thread ID of 0) so
//   that every application notifies the server when it repaints.  A global hook
//   is implemented by injecting the DLL into each hooked process.  Under Win32s
//   all Win32 apps share one VM and one 16-bit Windows 3.1 message queue, and a
//   32-bit DLL cannot be injected into a 16-bit task - which is what every
//   Windows 3.1 application is.  WH_SYSMSGFILTER and global WH_CALLWNDPROC are
//   also outside the Win32s USER32 subset.
//
//   There is no dynamic-import workaround for this.  The capability is absent.
//
// CONSEQUENCE: the server detects screen changes by polling.  SetHook() returns
// FALSE, and vncDesktop::ActivateHooks (vncDesktop.cpp) already handles that by
// logging the failure and calling m_server->PollFullScreen(TRUE).  That is the
// normal operating mode for this port, not an error condition.
//
// The keyboard/mouse FILTER and PRIORITY hooks are a separate feature ("disable
// local input while a client is connected" / "local input takes priority").
// They also require global hooks, so they also cannot work.  They return FALSE,
// and the two features are simply unavailable - the server treats a failed
// filter hook as "local input stays enabled", which is the safe direction.
//
// Everything here is deliberately trivial and side-effect free.  Do not add
// SetWindowsHookEx calls: a *thread-local* hook would install successfully and
// then silently do nothing useful, which is worse than a clean FALSE.
// ---------------------------------------------------------------------------

#include "stdhdrs.h"
#include "VNCHooks\VNCHooks.h"

extern "C" {

// ---------------------------------------------------------------------------
// Update notification hooks.
//
// Returning FALSE makes vncDesktop switch to full-screen polling.  See
// vncDesktop::ActivateHooks.
// ---------------------------------------------------------------------------

BOOL SetHook(HWND hWnd, UINT UpdateMsg, UINT CopyMsg, UINT MouseMsg)
{
	// Unused parameters, named for documentation:
	//   hWnd      - the desktop sink window that would receive notifications
	//   UpdateMsg - RFB_SCREEN_UPDATE
	//   CopyMsg   - RFB_COPYRECT_UPDATE
	//   MouseMsg  - RFB_MOUSE_UPDATE
	vnclog.Print(LL_INTINFO,
		VNCLOG("VNCHooks disabled on this platform - using polling\n"));
	return FALSE;
}

BOOL UnSetHook(HWND hWnd)
{
	// Return TRUE so that vncDesktop::ShutdownHooks computes
	// m_hooks_active = !UnSetHook(...) = FALSE, i.e. "no hooks active".
	// Returning FALSE here would leave m_hooks_active TRUE forever and make
	// ActivateHooks believe hooks were already installed.
	return TRUE;
}

// ---------------------------------------------------------------------------
// Local input suppression ("Disable local inputs while a client is connected").
//
// FALSE means the feature is unavailable.  vncDesktop calls these
// unconditionally from Startup() and does not check the result, so the effect is
// simply that local keyboard and mouse remain live - which is the correct
// failure direction: the alternative would be a machine whose input is dead with
// no way to re-enable it.
// ---------------------------------------------------------------------------

BOOL SetKeyboardFilterHook(BOOL activate)
{
	return FALSE;
}

BOOL SetMouseFilterHook(BOOL activate)
{
	return FALSE;
}

// ---------------------------------------------------------------------------
// Local input priority (Win9x variants).
//
// These would post RFB_LOCAL_KEYBOARD / RFB_LOCAL_MOUSE to the desktop window
// when the local user touches the machine, so that remote input is temporarily
// ignored.  Without them, vncServer::LocalInputPriority() has no source of
// events and the feature is inert.
// ---------------------------------------------------------------------------

BOOL SetKeyboardPriorityHook(HWND hwnd, BOOL activate, UINT LocalMsg)
{
	return FALSE;
}

BOOL SetMousePriorityHook(HWND hwnd, BOOL activate, UINT LocalMsg)
{
	return FALSE;
}

// ---------------------------------------------------------------------------
// Local input priority (NT low-level variants).
//
// WH_KEYBOARD_LL / WH_MOUSE_LL are NT 4.0+ only and do not exist on Win32s at
// all.
// ---------------------------------------------------------------------------

BOOL SetKeyboardPriorityLLHook(HWND hwnd, BOOL activate, UINT LocalMsg)
{
	return FALSE;
}

BOOL SetMousePriorityLLHook(HWND hwnd, BOOL activate, UINT LocalMsg)
{
	return FALSE;
}

} // extern "C"
