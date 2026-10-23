/////////////////////////////////////////////////////////////////////////////
// RFB LIBRARY / VNC Hooks library
//
// WinVNC uses this DLL to hook into the system message pipeline, allowing it
// to intercept messages which may be relevant to screen update strategy
//
// This library was originally created using MFC as RFBLib.dll, on
// 8/8/97	WEZ
//  Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
//
//  This file is part of the VNC system.
//
//  The VNC system is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
//  USA.
//
// TightVNC distribution homepage on the Web: http://www.tightvnc.com/
//
// If the source code for the VNC system is not available from the place 
// whence you received this file, check http://www.uk.research.att.com/vnc or contact
// the authors on vnc@uk.research.att.com for information on obtaining it.

// VNC Hooks library
//
// This version created:
// 24/11/97

// ==========================================================================
// WIN32S / WINDOWS 3.1 PORT - VNCHOOKS IS NOT USED
// ==========================================================================
//
// On this target the hook DLL cannot work, and the reason is structural rather
// than a missing API:
//
//   * VNCHooks installs SYSTEM-WIDE hooks - SetWindowsHookEx(WH_CALLWNDPROC,
//     ..., 0) and the same for WH_GETMESSAGE and WH_SYSMSGFILTER - so that every
//     application posts RFB_SCREEN_UPDATE to the server when it repaints.  That
//     is how the server normally learns what changed without scanning the
//     screen.
//
//   * A global hook works by injecting the hook DLL into each hooked process.
//     Win32s runs all Win32 applications in a single VM sharing the Windows 3.1
//     16-bit message queue, and a 32-bit Win32s DLL cannot be injected into a
//     16-bit task.  On Windows 3.1 essentially every application IS 16-bit, so
//     there is nothing the hooks could reach even if they installed.
//
//   * Even for the Win32s VM itself, WH_SYSMSGFILTER and system-wide
//     WH_CALLWNDPROC are not supported by the Win32s USER32 subset.
//
// VC++ 2.x / NT 3.1 TREE: the DLL IS built again (VNCHooks\vc2.mak), because
// on Windows NT - 3.1 included - global WH_CALLWNDPROC / WH_GETMESSAGE /
// WH_SYSMSGFILTER hooks work normally.  WinVNC does NOT link VNCHooks.lib:
//
//   * winvnc/VNCHooksStub.cpp defines these eight entry points as forwarders.
//     On NT it LoadLibrary()s VNCHooks.dll and calls through GetProcAddress;
//     on Win32s, or when the DLL is missing, it keeps the old stub behaviour.
//
//   * In the stub case SetHook() returns FALSE, which vncDesktop::ActivateHooks
//     already handles - it logs the failure and calls
//     m_server->PollFullScreen(TRUE), i.e. it falls back to polling the whole
//     screen.  That is the normal operating mode on Win32s.
//
// Exports are named in VNCHooks.def rather than with __declspec(dllexport), so
// the names GetProcAddress looks up are the plain extern "C" names whatever the
// compiler's decoration rules.  DllExport is therefore empty.
// ==========================================================================

#if !defined(_VNCHOOKS_DLL_)
#define _VNCHOOKS_DLL_

#include <windows.h>

/////////////////////////////////////////////////////////////////////////////
//
// Functions used by WinVNC.
//
// Implemented for real in VNCHooks.cpp (the DLL) and as run-time forwarders in
// winvnc/VNCHooksStub.cpp (WinVNC.exe).  Each binary sees one definition.

#ifndef DllExport
#define DllExport
#endif

extern "C"
{
	BOOL SetHook(
		HWND hWnd,
		UINT UpdateMsg,
		UINT CopyMsg,
		UINT MouseMsg
		);											// Set the hook
	BOOL UnSetHook(HWND hWnd);						// Remove it
	
	// Control keyboard filtering
	BOOL SetKeyboardFilterHook(BOOL activate);
	// Control mouse filtering
	BOOL SetMouseFilterHook(BOOL activate);
	// hooks for Local event priority impl. (win9x)
	BOOL SetKeyboardPriorityHook(HWND hwnd, BOOL activate,UINT LocalMsg);
	BOOL SetMousePriorityHook(HWND hwnd, BOOL activate,UINT LocalMsg);
	// hooks for Local event priority impl. (winNT)
	BOOL SetKeyboardPriorityLLHook(HWND hwnd, BOOL activate,UINT LocalMsg);
	BOOL SetMousePriorityLLHook(HWND hwnd, BOOL activate,UINT LocalMsg);

}

#endif // !defined(_VNCHOOKS_DLL_)
