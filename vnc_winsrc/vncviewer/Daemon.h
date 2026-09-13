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


#ifndef DAEMON_H__
#define DAEMON_H__

#pragma once

#include "stdhdrs.h"

// ==========================================================================
// WIN32S NOTE
//
// This header used to declare Shell_NotifyIcon itself:
//
//     BOOL __stdcall Shell_NotifyIcon(DWORD, PNOTIFYICONDATA);
//
// which put "Shell_NotifyIcon" in the EXE's import table.  Windows 3.1 has no
// system tray and the Win32s SHELL32 does not export it, so the loader could
// not start the process - the viewer died before WinMain with no diagnostic.
//
// NOTIFYICONDATA/NIM_*/NIF_* and the run-time-resolved wrapper
// Win32sShellNotifyIcon() now live in Win32sApi.h, so there is exactly one
// definition and no load-time dependency.
// ==========================================================================
#include "Win32sApi.h"

class Daemon  
{
public:
	Daemon(int port);
	virtual ~Daemon();
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
protected:
	// Real handler; WndProc is a try/catch wrapper around it so that no
	// exception is thrown across the USER32 dispatch frame.
	static LRESULT WndProcImpl(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);

	void AddTrayIcon();
	void CheckTrayIcon();
	void RemoveTrayIcon();
	bool SendTrayMsg(DWORD msg);

	// Show the popup menu at the given screen position.  Used both from the
	// tray icon (Win9x/NT) and from a click on the fallback window (Win32s).
	void ShowPopupMenu(int x, int y);

	SOCKET m_sock;
	HWND m_hwnd;
	HMENU m_hmenu;
	UINT m_timer;
	NOTIFYICONDATA m_nid;
	char netbuf[1024];

	// True when there is no system tray (Win32s / Windows 3.1) and the daemon
	// window itself is shown as the user's only handle on listening mode.
	bool m_noTray;
};

#endif // DAEMON_H__

