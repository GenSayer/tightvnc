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


// Daemon.cpp: implementation of the Daemon class.

#include "stdhdrs.h"
#include "vncviewer.h"
#include "Daemon.h"
#include "Exception.h"
#include "ClientConnection.h"
#include "AboutBox.h"
#include "Win32sApi.h"

// ==========================================================================
// WIN32S NOTE ON THE BLOCK THAT USED TO BE HERE
//
// This file used to declare, and therefore import, two Win95-only APIs:
//
//     ATOM __stdcall RegisterClassExA(CONST WNDCLASSEXA *);
//     BOOL __stdcall SetMenuDefaultItem(HMENU, UINT, UINT);
//
// plus a hand-written WNDCLASSEXA and "#define RegisterClassEx RegisterClassExA".
//
// RegisterClassEx is a Win95/NT API: it does not exist on Win32s at all.  With
// the name in the import table the EXE cannot be loaded on Win32s, which is one
// of the reasons the viewer died at startup with no message.
//
// The fix is not to resolve RegisterClassEx dynamically - it is to stop needing
// it.  The only thing WNDCLASSEX gives this window is hIconSm, which Windows
// 3.1 has no concept of.  The daemon window is now registered with the plain
// RegisterClass(), which exists everywhere.
//
// SetMenuDefaultItem is resolved at run time via Win32sSetMenuDefaultItem().
// ==========================================================================

// SD_BOTH now comes from win32s_fix.h (force-included), so there is one
// definition of it rather than one per file.


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
#define DAEMON_CLASS_NAME "VNCviewer Daemon"

Daemon::Daemon(int port)
{
	m_noTray = Win32sIsWin32s();
	m_timer  = 0;
	m_hmenu  = NULL;
	m_hwnd   = NULL;
	m_sock   = INVALID_SOCKET;
	memset(&m_nid, 0, sizeof(m_nid));

	// Create the daemon window.  Plain WNDCLASS / RegisterClass: see the note
	// above about RegisterClassEx being absent on Win32s.
	WNDCLASS wndclass;

	wndclass.style			= CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wndclass.lpfnWndProc	= Daemon::WndProc;
	wndclass.cbClsExtra		= 0;
	wndclass.cbWndExtra		= 0;
	wndclass.hInstance		= pApp->m_instance;
	wndclass.hIcon			= LoadIcon(pApp->m_instance, MAKEINTRESOURCE(IDR_TRAY));
	if (wndclass.hIcon == NULL)
		wndclass.hIcon		= LoadIcon(NULL, IDI_APPLICATION);
	wndclass.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground	= (HBRUSH) GetStockObject(WHITE_BRUSH);
	wndclass.lpszMenuName	= (const char *) NULL;
	wndclass.lpszClassName	= DAEMON_CLASS_NAME;

	RegisterClass(&wndclass);

	// On Win9x/NT the window is only a message sink for the tray icon and
	// stays hidden.  On Win32s there is no tray, so it has to be a real
	// (small) window or the user has no way to reach the menu or to quit.
	DWORD style = m_noTray
		? (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)
		: WS_OVERLAPPEDWINDOW;

	m_hwnd = CreateWindow(DAEMON_CLASS_NAME,
				DAEMON_CLASS_NAME,
				style,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				200, 200,
				NULL,
				NULL,
				pApp->m_instance,
				NULL);
	if (m_hwnd == NULL)
		throw WarningException("Error creating Daemon window");
	
	// record which client created this window
	SetWindowLong(m_hwnd, GWL_USERDATA, (LONG) this);

	// Load a popup menu
	m_hmenu = LoadMenu(pApp->m_instance, MAKEINTRESOURCE(IDR_TRAYMENU));

	// Create a listening socket
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    m_sock = socket(AF_INET, SOCK_STREAM, 0);
	// Was "if (!m_sock)": socket() returns INVALID_SOCKET (~0) on failure, not
	// 0, so the old test never fired and the code went on to bind a bad socket.
	if (m_sock == INVALID_SOCKET)
		throw WarningException("Error creating Daemon socket");

	int res = 0;

	res = bind(m_sock, (struct sockaddr *)&addr, sizeof(addr));
	if (res == SOCKET_ERROR) {
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
		throw WarningException("Error binding Daemon socket");
	}

	res = listen(m_sock, 5);
	if (res == SOCKET_ERROR) {
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
		throw WarningException("Error when Daemon listens");
	}

	// Send a message to specified window on an incoming connection.
	//
	// WSAAsyncSelect works on Win32s (it is a WinSock 1.1 call) and is exactly
	// the right mechanism for a single-threaded listener: the stack posts
	// WM_SOCKEVENT to our window instead of us blocking in accept().
	if (WSAAsyncSelect(m_sock, m_hwnd, WM_SOCKEVENT, FD_ACCEPT) == SOCKET_ERROR) {
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
		throw WarningException("Error setting up asynchronous socket notification");
	}

	if (m_noTray) {
		// No tray: show the window so listening mode is visible and closable.
		ShowWindow(m_hwnd, SW_SHOWMINNOACTIVE);
	} else {
		// Create the tray icon and start the timer that checks it survives an
		// Explorer restart.
		AddTrayIcon();
		m_timer = SetTimer(m_hwnd, IDT_TRAYTIMER, 15000, NULL);
	}
}

void Daemon::AddTrayIcon() {
	if (m_noTray)
		return;
	vnclog.Print(4, _T("Adding tray icon\n"));
	SendTrayMsg(NIM_ADD);
}

void Daemon::CheckTrayIcon() {
	if (m_noTray)
		return;
	vnclog.Print(8, _T("Checking tray icon\n"));
	if (!SendTrayMsg(NIM_MODIFY)) {
		vnclog.Print(4, _T("Tray icon not there - reinstalling\n"));
		AddTrayIcon();
	};
}

void Daemon::RemoveTrayIcon() {
	if (m_noTray)
		return;
	vnclog.Print(4, _T("Deleting tray icon\n"));
	SendTrayMsg(NIM_DELETE);
}

bool Daemon::SendTrayMsg(DWORD msg)
{
	if (m_noTray)
		return false;

	m_nid.hWnd = m_hwnd;
	m_nid.cbSize = sizeof(m_nid);
	m_nid.uID = IDR_TRAY;	// never changes after construction
	m_nid.hIcon = LoadIcon(pApp->m_instance, MAKEINTRESOURCE(IDR_TRAY));
	m_nid.uFlags = NIF_ICON | NIF_MESSAGE;
	m_nid.uCallbackMessage = WM_TRAYNOTIFY;
	m_nid.szTip[0] = '\0';
	// Use resource string as tip if there is one
	if (LoadString(pApp->m_instance, IDR_TRAY, m_nid.szTip, sizeof(m_nid.szTip))) {
		m_nid.uFlags |= NIF_TIP;
	}
	// Resolved at run time; returns FALSE where there is no tray.
	return (bool) (Win32sShellNotifyIcon(msg, &m_nid) != 0);
}

//
// Show the daemon popup menu.  Shared by the tray icon path and, on Win32s,
// by a right-click (or double-click) in the visible daemon window.
//
void Daemon::ShowPopupMenu(int x, int y)
{
	HMENU hSubMenu = GetSubMenu(m_hmenu, 0);
	if (hSubMenu == NULL) {
		vnclog.Print(2, _T("No systray submenu\n"));
		return;
	}

	// Make first menu item the default (bold font).  No-op on Win32s.
	Win32sSetMenuDefaultItem(hSubMenu, 0, TRUE);

	// The Win95 "SetForegroundWindow before TrackPopupMenu" workaround
	// (MSDN Q135788).  Win32sSetForegroundWindow falls back to
	// BringWindowToTop/SetActiveWindow on 3.1.
	Win32sSetForegroundWindow(m_hwnd);
	::TrackPopupMenu(hSubMenu, 0, x, y, 0, m_hwnd, NULL);
}

// Process window messages
//
// Wrapper: catches every exception so that nothing is thrown across the USER32
// frame that dispatched the message (MSVC 4.1 cannot unwind through it).  This
// procedure calls pApp->NewConnection() and VNCOptions::DoDialog(), both of
// which can throw.
LRESULT CALLBACK Daemon::WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam) {
	try {
		return WndProcImpl(hwnd, iMsg, wParam, lParam);
	} catch (WarningException &e) {
		e.Report();
	} catch (QuietException &e) {
		e.Report();
	} catch (Exception &e) {
		e.Report();
	}
	return 0;
}

LRESULT Daemon::WndProcImpl(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam) {
	// This is a static method, so we don't know which instantiation we're 
	// dealing with. We have stored a pseudo-this in the window user data, 
	// though.
	//
	// Unlike the ClientConnection procedures, _this is deliberately NOT required
	// here: WM_CREATE and the other creation-time messages arrive before
	// SetWindowLong runs, and WM_COMMAND from the popup menu must still work
	// after the destructor has cleared the back-pointer.  Every use of _this
	// below is individually checked.
	Daemon *_this = (Daemon *) GetWindowLong(hwnd, GWL_USERDATA);

	switch (iMsg) {

	case WM_CREATE:
		{
			return 0;
		}

	case WM_SOCKEVENT:
		{
			// Was: assert(HIWORD(lParam) == 0);
			// HIWORD(lParam) is the WinSock error code, and it is NOT always
			// zero - a failed accept notification carries one.  On a debug
			// build that assert aborts the process; on release it is compiled
			// out and the error is ignored.  Handle it properly instead.
			if (_this == NULL)
				return 0;

			int sockErr = (int)HIWORD(lParam);
			int sockEvt = (int)LOWORD(lParam);

			// A new socket created by accept might send messages to
			// this procedure. We can ignore them.
			if(wParam != _this->m_sock) {
				return 0;
			}

			if (sockErr != 0) {
				vnclog.Print(2, _T("Daemon socket event error %d\n"), sockErr);
				return 0;
			}

			switch(sockEvt) {
			case FD_ACCEPT:
				{
					SOCKET hNewSock;
					hNewSock = accept(_this->m_sock, NULL, NULL);
					if (hNewSock == INVALID_SOCKET) {
						vnclog.Print(2, _T("Daemon accept failed: %d\n"),
									 WSAGetLastError());
						break;
					}
					// Cancel async notification and put the socket back into
					// blocking mode.  WSAAsyncSelect implicitly makes a socket
					// non-blocking, and the protocol code (ReadExact) needs a
					// blocking socket.  Both calls are required, in this order.
					WSAAsyncSelect(hNewSock, hwnd, 0, 0);
					unsigned long nbarg = 0;
					ioctlsocket(hNewSock, FIONBIO, &nbarg);

					pApp->NewConnection(hNewSock);
					
					break;
				}
			case FD_READ:
				{
					unsigned long numbytes = 0;
					ioctlsocket(_this->m_sock, FIONREAD, &numbytes);
					if (numbytes > sizeof(_this->netbuf))
						numbytes = sizeof(_this->netbuf);
					if (numbytes > 0)
						recv(_this->m_sock, _this->netbuf, (int)numbytes, 0);
					break;
				}
			case FD_CLOSE:
				vnclog.Print(5, _T("Daemon connection closed\n"));
				DestroyWindow(hwnd);
				break;
			}
			
			return 0;
		}
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case ID_NEWCONN:
			pApp->NewConnection();
			break;
		case IDC_OPTIONBUTTON:
			// Only save if the user pressed OK.  The old code saved
			// unconditionally, so pressing Cancel still wrote the (unchanged,
			// but re-serialised) options - and on Win32s, where DoDialog used to
			// fail outright, it wrote them without the user seeing a dialog.
			if (pApp->m_options.DoDialog())
				pApp->m_options.SaveOpt(".listen", KEY_VNCVIEWER_HISTORI);
			break;
		case ID_CLOSEDAEMON:
			PostQuitMessage(0);
			break;
		case IDD_APP_ABOUT:
			ShowAboutBox();
			break;
		}
		return 0;
	case WM_TRAYNOTIFY:
		{
			if (_this == NULL)
				return 0;
			HMENU hSubMenu = GetSubMenu(_this->m_hmenu, 0);
			if (lParam==WM_LBUTTONDBLCLK) {
				// double click: execute first menu item
				if (hSubMenu != NULL)
					::SendMessage(_this->m_hwnd, WM_COMMAND, 
						GetMenuItemID(hSubMenu, 0), 0);
			} else if (lParam==WM_RBUTTONUP) {
				POINT mouse;
				GetCursorPos(&mouse);
				_this->ShowPopupMenu(mouse.x, mouse.y);
			} 
			return 0;
		}

	// Win32s has no tray, so the daemon window itself is the user interface.
	// Right-click (or double-click) anywhere in it to get the same menu.
	case WM_RBUTTONUP:
	case WM_LBUTTONDBLCLK:
		{
			if (_this == NULL || !_this->m_noTray)
				break;
			POINT mouse;
			GetCursorPos(&mouse);
			_this->ShowPopupMenu(mouse.x, mouse.y);
			return 0;
		}

	case WM_TIMER:
		if (_this != NULL)
			_this->CheckTrayIcon();
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	
	return DefWindowProc(hwnd, iMsg, wParam, lParam);
}

Daemon::~Daemon()
{
	if (m_timer != 0)
		KillTimer(m_hwnd, m_timer);
	RemoveTrayIcon();
	if (m_hmenu != NULL)
		DestroyMenu(m_hmenu);
	if (m_sock != INVALID_SOCKET) {
		// Stop async notification before closing, or the stack can post a
		// WM_SOCKEVENT for a window that is going away.
		WSAAsyncSelect(m_sock, m_hwnd, 0, 0);
		shutdown(m_sock, SD_BOTH);
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
	}
	if (m_hwnd != NULL) {
		// Clear the back-pointer first: DestroyWindow sends WM_DESTROY
		// synchronously and the handler must not use a half-destructed object.
		SetWindowLong(m_hwnd, GWL_USERDATA, (LONG)0);
		HWND h = m_hwnd;
		m_hwnd = NULL;
		DestroyWindow(h);
	}
}
