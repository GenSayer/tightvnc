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


// vncInstHandler.cpp

// Implementation of the class used to ensure that only
// one instance is running

#include "stdhdrs.h"
#include "vncInstHandler.h"
#include "vncMenu.h"		// MENU_CLASS_NAME, for the FindWindow test in Init()

// Name of the mutex
#ifdef HORIZONLIVE
const char mutexname [] = "AppShareHost_Instance_Mutex";
#else
const char mutexname [] = "WinVNC_Win32_Instance_Mutex";
#endif
// The class methods

vncInstHandler::vncInstHandler() : m_mutex(NULL)
{
}

vncInstHandler::~vncInstHandler()
{
	// make sure mutex is cleared as we exit
	Release();
}

BOOL
vncInstHandler::Init()
{
	// ==================================================================
	// WIN32S: single-instance detection.
	//
	// The original used a NAMED MUTEX - CreateMutex(NULL, FALSE, name) followed
	// by GetLastError() == ERROR_ALREADY_EXISTS.  That is not dependable here:
	//
	//   * Win32s runs every Win32 application inside ONE VM sharing a single
	//     address space.  Its kernel-object emulation is minimal and there is no
	//     real cross-process namespace for the mutex name to live in, so the
	//     ERROR_ALREADY_EXISTS result cannot be relied on.
	//
	//   * A false negative means two servers bind the same listening port; a
	//     false positive means the server refuses to start at all.  Neither is
	//     acceptable for the primary check.
	//
	// FindWindow() on the menu window class is the reliable test on this
	// platform - and it is exactly the mechanism vncService::KillRunningCopy()
	// and PostToWinVNC() already use to reach a running instance.  If those can
	// find the window, so can we.
	//
	// The mutex is still created afterwards: it costs nothing and remains a
	// second line of defence on Win9x/NT, where it does work properly.
	// ==================================================================

	// Is there already a WinVNC menu window?
	if (FindWindow(MENU_CLASS_NAME, NULL) != NULL) {
		vnclog.Print(LL_INTERR,
			VNCLOG("another instance is already running (window found)\n"));
		return FALSE;
	}

	// Create the named mutex
	m_mutex = CreateMutex(NULL, FALSE, mutexname);
	if (m_mutex == NULL) {
		// On Win32s a failure here is NOT conclusive - the window test above is
		// the authoritative one - so do not refuse to start.  The original
		// returned FALSE, which on a platform with unreliable named mutexes
		// would have meant "WinVNC never starts" with a misleading message.
		vnclog.Print(LL_INTINFO,
			VNCLOG("could not create instance mutex (error %d) - continuing\n"),
			GetLastError());
		return TRUE;
	}

	// Check that the mutex didn't already exist
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		vnclog.Print(LL_INTERR,
			VNCLOG("another instance is already running (mutex exists)\n"));
		return FALSE;
	}

	return TRUE;
}

//
// allow mutex to be explicitely cleared
//

DWORD
vncInstHandler::Release() 
{
	// WIN32S: was a try/catch around CloseHandle(m_mutex), on the stated
	// assumption that "CloseHandle() will throw an exception when passed an
	// invalid handle" so that the second call - from the destructor, after the
	// caller had already released - could be swallowed.
	//
	// That is not how CloseHandle behaves.  It returns FALSE and sets
	// ERROR_INVALID_HANDLE.  The only exception involved is the breakpoint
	// exception NT raises for a bad handle WHEN RUNNING UNDER A DEBUGGER, and
	// that is a structured exception which catch(...) does not intercept unless
	// /EHa is in effect - it is not.  So the try/catch never did anything.
	//
	// It matters here because on Win32s closing an already-closed handle is
	// undefined and can disturb the shared handle table that the whole VM uses.
	// Track the handle instead, so a second call is a genuine no-op.
	if (m_mutex != NULL) {
		CloseHandle(m_mutex);
		m_mutex = NULL;
	}

	return GetLastError();
}
