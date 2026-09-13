/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright (C) 2007 Constantin Kaplinsky.  All Rights Reserved.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

// ==========================================================================
// TsSessions - WIN32S / WINDOWS 3.1 VERSION
// ==========================================================================
//
// Terminal Services session discovery.  The original resolved four Windows XP
// APIs dynamically through DynamicFn<>:
//
//     ProcessIdToSessionId          (kernel32)
//     WTSGetActiveConsoleSessionId  (kernel32)
//     WinStationConnectW            (winsta.dll)
//     LockWorkStation               (user32)
//
// Because it used DynamicFn (LoadLibrary + GetProcAddress), it did NOT create
// load-time imports, so unlike the mirror driver this file would have loaded
// safely on Win32s.  It is still replaced, for three reasons:
//
//  1. It cannot do anything useful.  Windows 3.1 has no Terminal Services, no
//     sessions and no console session; every API above is absent, so
//     inConsoleSession() would take the "invalid" path.
//
//  2. THE ORIGINAL WOULD HAVE BROKEN STARTUP.  Look at the logic:
//
//         ProcessSessionId::ProcessSessionId(...) { id = 0; if (!valid) return; }
//         ConsoleSessionId::ConsoleSessionId()   { ... else id = 0; }
//         bool inConsoleSession() { return console.id == mySessionId.id; }
//
//     Both ids default to 0, so inConsoleSession() happens to return true.  That
//     is the ONLY reason vncDesktop::Startup() would not have failed at its first
//     line ("Console is not session zero - reconnect to restore Console
//     session").  Relying on two independent failure paths coincidentally
//     agreeing is not something to leave in place - especially since
//     setConsoleSession() is called first, and on the original would log
//     "WinSta APIs missing" on every single connection.
//
//  3. "ProcessSessionId mySessionId;" is a FILE-SCOPE OBJECT.  Its constructor
//     runs before WinMain, and it called through a DynamicFn whose own
//     constructor (also file-scope, in this same file) performs LoadLibrary.
//     The relative initialisation order of two file-scope objects in the same
//     translation unit is defined - declaration order - but this is exactly the
//     pattern that killed the viewer at startup (omni_thread's init_t), and on
//     Win32s a LoadLibrary before WinMain is worth avoiding on principle.
//
// Everything here is now a compile-time constant with no I/O and no dynamic
// loading.  DynamicFn.h is no longer included.
// ==========================================================================

#include "stdhdrs.h"
#include "TsSessions.h"

// Session ids are always 0: there is exactly one "session", the machine itself.

ProcessSessionId::ProcessSessionId(DWORD processId) {
	id = 0;
}

ProcessSessionId mySessionId;

ConsoleSessionId::ConsoleSessionId() {
	id = 0;
}

bool inConsoleSession() {
	// IMPORTANT: must return true.
	//
	// vncDesktop::Startup() begins with
	//     if (!inConsoleSession()) { ...log...; return FALSE; }
	// so returning false here would make every connection fail with a message
	// about restoring the console session, which would be meaningless on this
	// platform.
	//
	// The single Windows 3.1 desktop IS the console, so true is not a fudge - it
	// is the correct answer.
	return true;
}

void setConsoleSession(DWORD sessionId) {
	// Nothing to connect: there is only one session and we are already it.
	//
	// The original logged "WinSta APIs missing" here, which on this platform
	// would appear on every connection attempt.  Deliberately silent.
}
