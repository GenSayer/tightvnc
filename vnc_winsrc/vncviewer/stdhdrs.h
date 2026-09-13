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


#define VC_EXTRALEAN


#ifndef WINVER
#define WINVER 0x030A
#endif

// WAS: "#define WM_MOUSEWHEEEL 0x020A" - four E's.  The guard tested
// WM_MOUSEWHEEL and then defined a *differently spelled* symbol, so
// WM_MOUSEWHEEL still did not exist.  ClientConnection.cpp got away with it
// only because it defined WM_MOUSEWHEEL again in the middle of its own switch
// statement (now moved to the top of that file, properly guarded).
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x020A
#endif

// The scrolling increment constant (1 notch = 120 units)
#ifndef WHEEL_DELTA
#define WHEEL_DELTA 120
#endif

// Unpacks the signed wheel rotation delta from wParam
#ifndef GET_WHEEL_DELTA_WPARAM
#define GET_WHEEL_DELTA_WPARAM(wParam) ((short)HIWORD(wParam))
#endif

// Unpacks the key state modifiers (e.g., Ctrl, Shift) from wParam.
// WAS: "((first)LOWORD(wParam))" - "first" is not a type and this macro would
// not compile if anything ever used it.  Nothing does today, which is the only
// reason the build succeeds.
#ifndef GET_KEYSTATE_WPARAM
#define GET_KEYSTATE_WPARAM(wParam) ((int)(short)LOWORD(wParam))
#endif

#include <winsock.h>
#include <stdio.h>
#include <process.h>
#include <assert.h>
#include <time.h>
#include <tchar.h>
#include <windows.h>
#include <io.h>
#include <commctrl.h>


 
#include "rfb.h"

extern const char* g_buildTime;

