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


/* #ifndef _WIN64
  #define WINVER 0x0400
  #define _WIN32_WINDOWS 0x0410
#else
  #define WINVER 0x0500
  #define _WIN32_WINNT 0x500
#endif */

// Manually define the mouse scroll macros
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x20A
#endif

#ifndef WHEEL_DELTA
#define WHEEL_DELTA 120
#endif

#ifndef WHEEL_PAGESCROLL
#define WHEEL_PAGESCROLL UINT_MAX
#endif

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <process.h>
#include <assert.h>
#ifndef __MINGW32__
#include <crtdbg.h>
#endif
#include <locale.h>
#include <time.h>
#include <tchar.h>
#include <windows.h>
#include <io.h>
 
#include "rfb.h"

extern const char* g_buildTime;

