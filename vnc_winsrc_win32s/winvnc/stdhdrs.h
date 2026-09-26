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


// ==========================================================================
// WIN32S / WINDOWS 3.1 CHANGES IN THIS HEADER
// ==========================================================================
//
//  1. #define STRICT is REMOVED.
//
//     With STRICT defined, windows.h declares HWND, HDC, HBITMAP and friends as
//     pointers to distinct dummy structs.  win32s_fix.h - which is
//     force-included before every other header via /FI - has to typedef HWND
//     and HANDLE itself (it needs them to declare NMHDR before windows.h has
//     been seen).  Those typedefs are only compatible with the NON-strict
//     declarations, where HWND is simply HANDLE, i.e. void*.  With STRICT they
//     become a hard type conflict.
//
//     Leaving STRICT off also matters for the Win16 thunk layer: many Win32s
//     handle values are really 16-bit handles widened to 32 bits, and code that
//     casts between HANDLE and integer types (of which VNC has plenty) does not
//     compile cleanly under STRICT with this compiler.
//
//  2. <winsock2.h> -> <winsock.h>.
//
//     Win32s ships WinSock 1.1 and wsock32.lib is a 1.1 import library.
//     winsock2.h does not exist in the MSVC 4.1 SDK at all, and including it
//     would declare WSAStartup with a 2.x version expectation.  This is the
//     same change already made in the viewer.
//
//  3. <crtdbg.h> is guarded.  It exists in MSVC 4.1 but the _RPTn macros it
//     provides are only active in a debug build; several files call _RPT0/_RPT3
//     unconditionally, so the header must still be reachable.
//
//  4. <ctype.h> and <string.h> are added.  vncService.cpp uses tolower() and
//     several files use strcpy/strlen without including them directly, relying
//     on another header having done so.
// ==========================================================================

// #define VC_EXTRALEAN
// #define STRICT		<- deliberately NOT defined; see note 1 above.

#include <winsock.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <process.h>
#include <crtdbg.h>

extern const char* g_buildTime;

// LOGGING SUPPORT

#include "Log.h"
#include "list.h"	// WIN32S: local minimal list<> (MSVC 4.1 STL cannot build these)
extern Log vnclog;

// No logging at all
#define LL_NONE		0
// Log server startup/shutdown
#define LL_STATE	0
// Log connect/disconnect
#define LL_CLIENTS	1
// Log connection errors (wrong pixfmt, etc)
#define LL_CONNERR	0
// Log socket errors
#define LL_SOCKERR	4
// Log internal errors
#define LL_INTERR	0

// Log internal warnings
#define LL_INTWARN	8
// Log internal info
#define LL_INTINFO	9
// Log socket errors
#define LL_SOCKINFO	10
// Log everything, including internal table setup, etc.
#define LL_ALL		10

