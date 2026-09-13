//  Copyright (C) 2004 HorizonWimba, Inc. All Rights Reserved.
//  Copyright (C) 2003-2006 Constantin Kaplinsky. All Rights Reserved.
//  Copyright (C) 2002 RealVNC Ltd. All Rights Reserved.
//  Copyright (C) 2000 Tridia Corporation. All Rights Reserved.
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


// vncMenu

// Implementation of a system tray icon & menu for WinVNC

#include "stdhdrs.h"
#include "WinVNC.h"
#include "vncService.h"
#include "vncConnDialog.h"
// WIN32S: <lmcons.h> removed - see the note in vncMenu.h.  UNLEN is defined
// there instead.
// #include <lmcons.h>

// Header

#include "vncMenu.h"

// Constants
const UINT MENU_PROPERTIES_SHOW = RegisterWindowMessage("WinVNC.Properties.User.Show");
const UINT MENU_SERVER_SHAREALL = RegisterWindowMessage("WinVNC.Server.ShareAll");
const UINT MENU_SERVER_SHAREPRIMARY = RegisterWindowMessage("WinVNC.Server.SharePrimary");
const UINT MENU_SERVER_SHAREAREA = RegisterWindowMessage("WinVNC.Server.ShareArea");
const UINT MENU_SERVER_SHAREWINDOW = RegisterWindowMessage("WinVNC.Server.ShareWindow");
const UINT MENU_DEFAULT_PROPERTIES_SHOW = RegisterWindowMessage("WinVNC.Properties.Default.Show");
const UINT MENU_ABOUTBOX_SHOW = RegisterWindowMessage("WinVNC.AboutBox.Show");
const UINT MENU_SERVICEHELPER_MSG = RegisterWindowMessage("WinVNC.ServiceHelper.Message");
const UINT MENU_RELOAD_MSG = RegisterWindowMessage("WinVNC.Reload.Message");
const UINT MENU_ADD_CLIENT_MSG = RegisterWindowMessage("WinVNC.AddClient.Message");
const UINT MENU_KILL_ALL_CLIENTS_MSG = RegisterWindowMessage("WinVNC.KillAllClients.Message");

const UINT fileTransferDownloadMessage = RegisterWindowMessage("VNCServer.1.3.FileTransferDownloadMessage");

const char *MENU_CLASS_NAME = "WinVNC Tray Icon";

// Implementation

vncMenu::vncMenu(vncServer *server)
{
	// Save the server pointer
	m_server = server;

	// Set the initial user name to something sensible...
	vncService::CurrentUser((char *)&m_username, sizeof(m_username));

	// WIN32S: is there a system tray at all?
	//
	// Windows 3.1 has no taskbar and no notification area.  Win32sIsWin32s()
	// tells us, and Win32sShellNotifyIcon() will return FALSE anyway because
	// SHELL32 does not export Shell_NotifyIcon there.
	//
	// Without a tray the user needs SOME way to reach the menu and to quit the
	// server, so the "dummy" window becomes a real (minimised) window that
	// responds to a click.  See the WM_RBUTTONUP / WM_LBUTTONDBLCLK handling in
	// WndProc.
	m_noTray = Win32sIsWin32s() ? TRUE : FALSE;

	// Create the window to handle tray icon messages.
	//
	// WIN32S: plain WNDCLASS / RegisterClass, not WNDCLASSEX / RegisterClassEx.
	// RegisterClassEx is Win95/NT only and is not exported by the Win32s USER32;
	// because the linker records it as an import, its presence prevents the EXE
	// from LOADING - the process never starts and there is no diagnostic.  The
	// only field WNDCLASSEX adds here is hIconSm, which Windows 3.1 has no
	// concept of.
	WNDCLASS wndclass;

	wndclass.style			= 0;
	wndclass.lpfnWndProc	= vncMenu::WndProc;
	wndclass.cbClsExtra		= 0;
	wndclass.cbWndExtra		= 0;
	wndclass.hInstance		= hAppInstance;
	wndclass.hIcon			= LoadIcon(hAppInstance, MAKEINTRESOURCE(IDI_WINVNC));
	if (wndclass.hIcon == NULL)
		wndclass.hIcon		= LoadIcon(NULL, IDI_APPLICATION);
	wndclass.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground	= (HBRUSH) GetStockObject(WHITE_BRUSH);
	wndclass.lpszMenuName	= (const char *) NULL;
	wndclass.lpszClassName	= MENU_CLASS_NAME;

	RegisterClass(&wndclass);

	// On Win9x/NT this window is only a message sink for the tray icon and stays
	// hidden.  With no tray it has to be visible (minimised) or the user has no
	// way to reach the menu.
	DWORD style = m_noTray
		? (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)
		: WS_OVERLAPPEDWINDOW;

	m_hwnd = CreateWindow(MENU_CLASS_NAME,
				"TightVNC Server",
				style,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				200, 200,
				NULL,
				NULL,
				hAppInstance,
				NULL);
	if (m_hwnd == NULL)
	{
		vnclog.Print(LL_INTERR, VNCLOG("unable to CreateWindow:%d\n"), GetLastError());
		PostQuitMessage(0);
		return;
	}

	// Timer to trigger icon updating.
	//
	// Pointless when there is no tray icon to update, and Windows 3.1 has a
	// small system-wide timer limit that the polling timer and the main loop's
	// idle timer also draw on - so do not consume one for nothing.
	if (!m_noTray)
		SetTimer(m_hwnd, 1, 5000, NULL);

	// record which client created this window
	SetWindowLong(m_hwnd, GWL_USERDATA, (LONG) this);

	// Ask the server object to notify us of stuff
	server->AddNotify(m_hwnd);

	// Load the icons for the tray
	m_winvnc_normal_icon = LoadIcon(hAppInstance, MAKEINTRESOURCE(IDI_WINVNC));
	m_winvnc_disabled_icon = LoadIcon(hAppInstance, MAKEINTRESOURCE(IDI_DISABLED));
	m_flash_icon = LoadIcon(hAppInstance, MAKEINTRESOURCE(IDI_FLASH));

	// Load the popup menu
	m_hmenu = LoadMenu(hAppInstance, MAKEINTRESOURCE(IDR_TRAYMENU));

	// Initialise the properties dialog object
	if (!m_properties.Init(m_server))
	{
		vnclog.Print(LL_INTERR, VNCLOG("unable to initialise Properties dialog\n"));
		PostQuitMessage(0);
		return;
	}

	// Install the tray icon, or show the window if there is no tray.
	if (m_noTray) {
		vnclog.Print(LL_STATE,
			VNCLOG("no system tray on this platform - showing server window\n"));
		ShowWindow(m_hwnd, SW_SHOWMINNOACTIVE);
	} else {
		AddTrayIcon();
	}
}

vncMenu::~vncMenu()
{
	// Remove the tray icon
	DelTrayIcon();
	
	// Destroy the loaded menu
	if (m_hmenu != NULL)
		DestroyMenu(m_hmenu);

	// Tell the server to stop notifying us!
	if (m_server != NULL)
		m_server->RemNotify(m_hwnd);
}

void
vncMenu::AddTrayIcon()
{
	if (m_noTray)
		return;

	// If the user name is empty, then we consider no user is logged in.
	if (strcmp(m_username, "") != 0) {
		// Make sure the server has not been configured to
		// suppress the tray icon.
		if (!m_server->GetDisableTrayIcon())
			SendTrayMsg(NIM_ADD, FALSE);
	}
}

void
vncMenu::DelTrayIcon()
{
	if (m_noTray)
		return;
	SendTrayMsg(NIM_DELETE, FALSE);
}

void
vncMenu::FlashTrayIcon(BOOL flash)
{
	if (m_noTray)
		return;
	SendTrayMsg(NIM_MODIFY, flash);
}

// Get the local ip addresses as a human-readable string.
// If more than one, then with \n between them.
// If not available, then gets a message to that effect.
void GetIPAddrString(char *buffer, int buflen) {
    char namebuf[256];

    if (gethostname(namebuf, 256) != 0) {
	strncpy(buffer, "Host name unavailable", buflen);
	return;
    };

    HOSTENT *ph = gethostbyname(namebuf);
    if (!ph) {
	strncpy(buffer, "IP address unavailable", buflen);
	return;
    };

    *buffer = '\0';
    char digtxt[5];
    for (int i = 0; ph->h_addr_list[i]; i++) {
    	for (int j = 0; j < ph->h_length; j++) {
	    sprintf(digtxt, "%d.", (unsigned char) ph->h_addr_list[i][j]);
	    strncat(buffer, digtxt, (buflen-1)-strlen(buffer));
	}	
	buffer[strlen(buffer)-1] = '\0';
	if (ph->h_addr_list[i+1] != 0)
	    strncat(buffer, ", ", (buflen-1)-strlen(buffer));
    }
}

void
vncMenu::SendTrayMsg(DWORD msg, BOOL flash)
{
	// Create the tray icon message
	m_nid.hWnd = m_hwnd;
	m_nid.cbSize = sizeof(m_nid);
	m_nid.uID = IDI_WINVNC;			// never changes after construction
	m_nid.hIcon = m_winvnc_normal_icon;
	m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	m_nid.uCallbackMessage = WM_TRAYNOTIFY;

	// Construct the tip string
	const char *title = (vncService::RunningAsService()) ?
		"TightVNC Service - " : "TightVNC Server - ";
	m_nid.szTip[0] = '\0';
	strncat(m_nid.szTip, title, sizeof(m_nid.szTip) - 1);
	if (m_server->SockConnected()) {
		size_t tiplen = strlen(m_nid.szTip);
		char *tipptr = ((char *)&m_nid.szTip) + tiplen;

		// Try to add the server's IP addresses to the tip string, if possible
		GetIPAddrString(tipptr, sizeof(m_nid.szTip) - tiplen);
		if (m_server->ClientsDisabled()) {
			m_nid.hIcon = m_winvnc_disabled_icon;
			strncat(m_nid.szTip, " (new clients disabled)",
					sizeof(m_nid.szTip) - 1 - strlen(m_nid.szTip));
		} else if (!m_server->ValidPasswordsSet()) {
			m_nid.hIcon = m_winvnc_disabled_icon;
			strncat(m_nid.szTip, " (no valid passwords set)",
					sizeof(m_nid.szTip) - 1 - strlen(m_nid.szTip));
		}
	} else {
		m_nid.hIcon = m_winvnc_disabled_icon;
		strncat(m_nid.szTip, "Not listening",
				sizeof(m_nid.szTip) - 1 - strlen(m_nid.szTip));
	}

	if (flash)
		m_nid.hIcon = m_flash_icon;

	// Send the message.
	//
	// WIN32S: Win32sShellNotifyIcon resolves Shell_NotifyIconA at run time (see
	// Win32sApi.cpp).  Declaring Shell_NotifyIcon directly - as the original did
	// via shellapi.h - puts it in the import table, and SHELL32 on Win32s does
	// not export it, so the EXE would not load.
	if (Win32sShellNotifyIcon(msg, &m_nid))
	{
		// Set the enabled/disabled state of the menu items
		vnclog.Print(LL_INTINFO, VNCLOG("tray icon updated ok\n"));

		EnableMenuItem(m_hmenu, ID_PROPERTIES,
			m_properties.AllowProperties() ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(m_hmenu, ID_CLOSE,
			m_properties.AllowShutdown() ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(m_hmenu, ID_DISABLE_CONN,
			m_properties.AllowShutdown() ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(m_hmenu, ID_KILLCLIENTS,
			m_properties.AllowEditClients() ? MF_ENABLED : MF_GRAYED);
		EnableMenuItem(m_hmenu, ID_OUTGOING_CONN,
			m_properties.AllowEditClients() ? MF_ENABLED : MF_GRAYED);
	} else {
		if (!vncService::RunningAsService())
		{
			if (msg == NIM_ADD)
			{
				// The tray icon couldn't be created.
				//
				// WIN32S: the original opened the Properties dialog as a
				// substitute main window and then called PostQuitMessage(0) -
				// which quits the application as soon as that dialog closes.
				// That is reasonable on Win9x, where a failed Shell_NotifyIcon
				// means something is badly wrong, but here it would be the
				// NORMAL path if m_noTray were not set first.
				//
				// AddTrayIcon() now returns early when there is no tray, so this
				// branch only runs on a platform that should have had one.  Fall
				// back to showing our own window rather than quitting, so the
				// server stays usable either way.
				vnclog.Print(LL_INTERR,
					VNCLOG("unable to add tray icon (error %d) - showing window\n"),
					GetLastError());
				m_noTray = TRUE;
				ShowWindow(m_hwnd, SW_SHOWMINNOACTIVE);
			}
		}
	}
}

//
// ShowPopupMenu - display the server menu at the cursor.
//
// Factored out of the WM_TRAYNOTIFY handler so that the Win32s path (a click in
// the visible server window) and the tray path share one implementation.
//
// WIN32S notes on the two API calls:
//
//   SetMenuDefaultItem is Win95+ and absent from the Win32s USER32.
//   Win32sSetMenuDefaultItem resolves it at run time and does nothing when it is
//   unavailable - the first menu item simply is not bold.
//
//   SetForegroundWindow is likewise Win95+.  Win32sSetForegroundWindow falls
//   back to BringWindowToTop + SetActiveWindow.  The call is the MSDN Q135788
//   workaround: without it, TrackPopupMenu on Win95 leaves a menu that does not
//   dismiss when the user clicks elsewhere.
//
void
vncMenu::ShowPopupMenu()
{
	// Get the submenu to use as a pop-up menu
	HMENU submenu = GetSubMenu(m_hmenu, 0);
	if (submenu == NULL)
	{
		vnclog.Print(LL_INTERR, VNCLOG("no submenu available\n"));
		return;
	}

	// Make the first menu item the default (bold font)
	Win32sSetMenuDefaultItem(submenu, 0, TRUE);

	// Get the current cursor position, to display the menu at
	POINT mouse;
	GetCursorPos(&mouse);

	Win32sSetForegroundWindow(m_hwnd);

	// Display the menu at the desired position.
	//
	// NOTE: the original passed m_nid.hWnd here.  That is the same handle as
	// m_hwnd, but m_nid is only filled in by SendTrayMsg() - so on a platform
	// where the tray icon was never added, m_nid.hWnd was uninitialised and
	// TrackPopupMenu received garbage.  Use m_hwnd, which is always valid.
	TrackPopupMenu(submenu, 0, mouse.x, mouse.y, 0, m_hwnd, NULL);
}

// Process window messages
LRESULT CALLBACK vncMenu::WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	// This is a static method, so we don't know which instantiation we're 
	// dealing with. We use Allen Hadden's (ahadden@taratec.com) suggestion 
	// from a newsgroup to get the pseudo-this.
	vncMenu *_this = (vncMenu *) GetWindowLong(hwnd, GWL_USERDATA);

	// WIN32S: _this is NULL for every message delivered during CreateWindow -
	// WM_NCCREATE, WM_CREATE, WM_GETMINMAXINFO, WM_NCCALCSIZE - because
	// SetWindowLong(GWL_USERDATA) does not run until after CreateWindow returns.
	// Every case below dereferences it.
	//
	// This matters much more on this platform than on Win9x/NT, because here the
	// window is VISIBLE (there is no system tray), so it also receives
	// WM_PAINT/WM_SIZE/WM_ERASEBKGND during creation.
	if (_this == NULL)
		return DefWindowProc(hwnd, iMsg, wParam, lParam);

	switch (iMsg)
	{

	// ==================================================================
	// WIN32S: paint the window.
	//
	// On Win9x/NT this window is never shown, so it never needed a WM_PAINT
	// handler - DefWindowProc filling it with the class background brush was
	// enough.  Here it IS shown (minimised, restorable), and with no handler the
	// restored window is blank white, which is what "gives a blank screen instead
	// of the user properties screen" describes: the properties DIALOG is separate
	// and modal, and what remained on screen afterwards was this empty window.
	//
	// Draw something that tells the user what the window is and what to do with
	// it.
	// ==================================================================
	case WM_PAINT:
		if (_this->m_noTray) {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			if (hdc != NULL) {
				RECT rc;
				GetClientRect(hwnd, &rc);

				// Use the dialog font rather than the default System font, which
				// on Windows 3.1 is large and clips.
				HFONT hf = (HFONT)GetStockObject(ANSI_VAR_FONT);
				HFONT hfOld = (HFONT)SelectObject(hdc, hf);
				SetBkMode(hdc, TRANSPARENT);

				char line1[128];
				char line2[128];
				int nClients = _this->m_server->AuthClientCount();

				strcpy(line1, "TightVNC Server is running.");
				if (nClients > 0) {
					wsprintf(line2, "%d client%s connected.  Right-click for menu.",
							 nClients, (nClients == 1) ? "" : "s");
				} else {
					strcpy(line2, "Right-click here for the menu.");
				}

				RECT rt = rc;
				rt.top += 8;
				DrawText(hdc, line1, strlen(line1), &rt,
						 DT_CENTER | DT_TOP | DT_SINGLELINE);
				rt.top += 20;
				DrawText(hdc, line2, strlen(line2), &rt,
						 DT_CENTER | DT_TOP | DT_SINGLELINE);

				if (hfOld != NULL)
					SelectObject(hdc, hfOld);
				EndPaint(hwnd, &ps);
			}
			return 0;
		}
		break;

		// Every five seconds, a timer message causes the icon to update
	case WM_TIMER:
	    	// *** HACK for running servicified
		if (vncService::RunningAsService()) {
		    // Attempt to add the icon if it's not already there
		    _this->AddTrayIcon();
		    // Trigger a check of the current user
		    PostMessage(hwnd, WM_USERCHANGED, 0, 0);
		}

		// Update the icon
		_this->FlashTrayIcon(_this->m_server->AuthClientCount() != 0);
		break;

		// DEAL WITH NOTIFICATIONS FROM THE SERVER:

	case WM_SRV_CLIENT_AUTHENTICATED:
		_this->m_properties.ResetTabId();
		_this->FlashTrayIcon(TRUE);
		// WIN32S: the visible window shows the client count, so refresh it.
		if (_this->m_noTray)
			InvalidateRect(hwnd, NULL, TRUE);
		return 0;

	case WM_SRV_CLIENT_DISCONNECT:
		_this->m_properties.ResetTabId();
		if (_this->m_server->AuthClientCount() == 0) {
			_this->FlashTrayIcon(FALSE);
			_this->m_wputils.RestoreWallpaper();
		}
		if (_this->m_noTray)
			InvalidateRect(hwnd, NULL, TRUE);
		return 0;

	case WM_SRV_CLIENT_HIDEWALLPAPER:
		_this->m_wputils.KillWallpaper();
		_this->m_server->ClearWallpaperWait();
		return 0;

		// STANDARD MESSAGE HANDLING

	case WM_CREATE:
		return 0;

	case WM_COMMAND:
		// User has clicked an item on the tray menu
		switch (LOWORD(wParam))
		{

		case ID_DEFAULT_PROPERTIES:
			// Show the default properties dialog, unless it is already displayed
			vnclog.Print(LL_INTINFO, VNCLOG("show default properties requested\n"));
			_this->m_properties.Show(TRUE, FALSE);
			_this->FlashTrayIcon(_this->m_server->AuthClientCount() != 0);
			break;
		
		case ID_PROPERTIES:
			// Show the properties dialog, unless it is already displayed
			vnclog.Print(LL_INTINFO, VNCLOG("show user properties requested\n"));
			_this->m_properties.Show(TRUE, TRUE);
			_this->FlashTrayIcon(_this->m_server->AuthClientCount() != 0);
			break;
		
		case ID_OUTGOING_CONN:
			// Connect out to a listening VNC viewer
			{
				vncConnDialog *newconn = new vncConnDialog(_this->m_server);
				if (newconn)
					newconn->DoDialog();
			}
			break;

		case ID_KILLCLIENTS:
			// Disconnect all currently connected clients
			_this->m_server->KillAuthClients();
			break;

		case ID_DISABLE_CONN:
			// Disallow incoming connections (changed to leave existing ones)
			if (GetMenuState(_this->m_hmenu, ID_DISABLE_CONN, MF_BYCOMMAND) & MF_CHECKED)
			{
				_this->m_server->DisableClients(FALSE);
				CheckMenuItem(_this->m_hmenu, ID_DISABLE_CONN, MF_UNCHECKED);
			} else
			{
				_this->m_server->DisableClients(TRUE);
				CheckMenuItem(_this->m_hmenu, ID_DISABLE_CONN, MF_CHECKED);
			}
			// Update the icon
			_this->FlashTrayIcon(_this->m_server->AuthClientCount() != 0);
			break;

		case ID_ABOUT:
			// Show the About box
			_this->m_about.Show(TRUE);
			break;

		case ID_CLOSE:
			// User selected Close from the tray menu
			PostMessage(hwnd, WM_CLOSE, 0, 0);
			break;

		}
		return 0;

	case WM_TRAYNOTIFY:
		// User has clicked on the tray icon or the menu
		{
			// What event are we responding to, RMB click?
			if (lParam==WM_RBUTTONUP)
			{
				_this->ShowPopupMenu();
				return 0;
			}
			
			// Or was there a LMB double click?
			if (lParam==WM_LBUTTONDBLCLK)
			{
				// double click: execute first menu item
				HMENU submenu = GetSubMenu(_this->m_hmenu, 0);
				if (submenu != NULL)
					SendMessage(hwnd, WM_COMMAND,
								GetMenuItemID(submenu, 0), 0);
			}

			return 0;
		}

	// ==================================================================
	// WIN32S: with no system tray, the server's own window is the user's
	// only handle on it.  Right-click (or double-click) anywhere in it to
	// get the same menu the tray icon would have shown.
	//
	// These messages are simply never sent on Win9x/NT, because the window
	// stays hidden there.
	// ==================================================================
	case WM_RBUTTONUP:
		if (!_this->m_noTray)
			break;
		_this->ShowPopupMenu();
		return 0;

	case WM_LBUTTONDBLCLK:
		if (!_this->m_noTray)
			break;
		{
			HMENU submenu = GetSubMenu(_this->m_hmenu, 0);
			if (submenu != NULL)
				SendMessage(hwnd, WM_COMMAND, GetMenuItemID(submenu, 0), 0);
		}
		return 0;

	case WM_CLOSE:

		// Only accept WM_CLOSE if the logged on user has AllowShutdown set
		if (!_this->m_properties.AllowShutdown())
			return 0;
		_this->m_server->KillAuthClients();
		_this->m_wputils.RestoreWallpaper();
		break;

	case WM_DESTROY:
		// The user wants WinVNC to quit cleanly...
		vnclog.Print(LL_INTERR, VNCLOG("quitting from WM_DESTROY\n"));
		PostQuitMessage(0);
		return 0;

	case WM_QUERYENDSESSION:
		vnclog.Print(LL_INTERR, VNCLOG("WM_QUERYENDSESSION\n"));
		break;

	case WM_ENDSESSION:
		vnclog.Print(LL_INTERR, VNCLOG("WM_ENDSESSION\n"));
		break;

/*
	case WM_ENDSESSION:
		// Are we running as a system service, or shutting the system down?
		if (wParam && (lParam == 0))
		{
			// Shutdown!
			// Try to clean ourselves up nicely, if possible...

			// Firstly, disable incoming CORBA or socket connections
			_this->m_server->SockConnect(FALSE);
			_this->m_server->CORBAConnect(FALSE);

			// Now kill all the server's clients
			_this->m_server->KillAuthClients();
			_this->m_server->KillUnauthClients();
			_this->m_server->WaitUntilAuthEmpty();
			_this->m_server->WaitUntilUnauthEmpty();
		}

		// Tell the OS that we've handled it anyway
		return 0;
		*/

	case WM_USERCHANGED:
		// The current user may have changed.
		{
			char newuser[UNLEN+1];

			if (vncService::CurrentUser((char *) &newuser, sizeof(newuser)))
			{
				vnclog.Print(LL_INTINFO,
					VNCLOG("usernames : old=\"%s\", new=\"%s\"\n"),
					_this->m_username, newuser);

				// Check whether the user name has changed!
				if (strcmp(newuser, _this->m_username) != 0)
				{
					vnclog.Print(LL_INTINFO,
						VNCLOG("user name has changed\n"));

					// User has changed!
					strcpy(_this->m_username, newuser);

					// Redraw the tray icon and set it's state
					_this->DelTrayIcon();
					_this->AddTrayIcon();
					_this->FlashTrayIcon(_this->m_server->AuthClientCount() != 0);

					// We should load in the prefs for the new user
					_this->m_properties.Load(TRUE);
				}
			}
		}
		return 0;
	
	default:
		// Deal with any of our custom message types
		if (iMsg == MENU_PROPERTIES_SHOW)
		{
			// External request to show our Properties dialog
			PostMessage(hwnd, WM_COMMAND, MAKELONG(ID_PROPERTIES, 0), 0);
			return 0;
		}
		if (iMsg == MENU_SERVER_SHAREALL)
		{
			_this->m_properties.HideMatchWindow();
			_this->m_server->FullScreen(true);
			_this->m_server->PrimaryDisplayOnlyShared(false);
			_this->m_server->ScreenAreaShared(false);
			_this->m_server->WindowShared(false);
			return 0;
		}
		if (iMsg == MENU_SERVER_SHAREPRIMARY)
		{
			_this->m_properties.HideMatchWindow();
			_this->m_server->FullScreen(false);
			_this->m_server->PrimaryDisplayOnlyShared(true);
			_this->m_server->ScreenAreaShared(false);
			_this->m_server->WindowShared(false);
			return 0;
		}
		if (iMsg == MENU_SERVER_SHAREAREA)
		{
			int left = LOWORD(wParam);
			int right = left + LOWORD(lParam);
			int top = HIWORD(wParam);
			int bottom = top + HIWORD(lParam);
			_this->m_properties.MoveMatchWindow(left, top, right, bottom);
			_this->m_properties.ShowMatchWindow();
			_this->m_server->SetMatchSizeFields(left, top, right, bottom);
			_this->m_server->FullScreen(false);
			_this->m_server->PrimaryDisplayOnlyShared(false);
			_this->m_server->ScreenAreaShared(true);
			_this->m_server->WindowShared(false);
			return 0;
		}
		if (iMsg == MENU_SERVER_SHAREWINDOW)
		{
			HWND hWindowShared = (HWND)wParam;
			if (hWindowShared != NULL)
			{
				_this->m_properties.HideMatchWindow();
				_this->m_server->SetWindowShared(hWindowShared);
				_this->m_server->FullScreen(false);
				_this->m_server->PrimaryDisplayOnlyShared(false);
				_this->m_server->ScreenAreaShared(false);
				_this->m_server->WindowShared(true);
			}
			return 0;
		}
		if (iMsg == MENU_DEFAULT_PROPERTIES_SHOW)
		{
			// External request to show our Properties dialog
			PostMessage(hwnd, WM_COMMAND, MAKELONG(ID_DEFAULT_PROPERTIES, 0), 0);
			return 0;
		}
		if (iMsg == MENU_ABOUTBOX_SHOW)
		{
			// External request to show our About dialog
			PostMessage(hwnd, WM_COMMAND, MAKELONG(ID_ABOUT, 0), 0);
			return 0;
		}
		if (iMsg == MENU_SERVICEHELPER_MSG)
		{
			// External ServiceHelper message.
			// This message holds a process id which we can use to
			// impersonate a specific user.  In doing so, we can load their
			// preferences correctly
			DWORD processId = (DWORD)lParam;
			vncService::ProcessUserHelperMessage(processId);

			// - Trigger a check of the current user
			PostMessage(hwnd, WM_USERCHANGED, 0, 0);
			return 0;
		}
		if (iMsg == MENU_RELOAD_MSG)
		{
			// Reload the preferences
			_this->m_properties.Load(TRUE);

			// Redraw the tray icon and set it's state
			_this->DelTrayIcon();
			_this->AddTrayIcon();
			_this->FlashTrayIcon(_this->m_server->AuthClientCount() != 0);

			return 0;
		}
		if (iMsg == MENU_ADD_CLIENT_MSG)
		{
			// handle add client mesage instigated by '-connect' flag
			// and/or call to vncService::PostAddNewClient() 
			
			// Add Client message.  This message includes an IP address and
			// a port numbrt of a listening client, to which we should connect.

			// If there is no IP address then show the connection dialog
			if (!lParam) {
				vncConnDialog *newconn = new vncConnDialog(_this->m_server);
				if (newconn) newconn->DoDialog();
				return 0;
			}

			// Get the IP address stringified
			struct in_addr address;
			address.S_un.S_addr = lParam;
			char *name = inet_ntoa(address);
			if (name == 0)
				return 0;
			char *nameDup = strdup(name);
			if (nameDup == 0)
				return 0;

			// Get the port number
			unsigned short nport = (unsigned short)wParam;

			// Attempt to create a new socket
			VSocket *tmpsock;
			tmpsock = new VSocket;
			if (tmpsock) {
				// Connect out to the specified host on the VNCviewer listen port
				tmpsock->Create();
				if (tmpsock->Connect(nameDup, nport)) {
					// Add the new client to this server
					_this->m_server->AddClient(tmpsock, TRUE, TRUE);
				} else {
					delete tmpsock;
				}
			}

			// Free the duplicate name
			free(nameDup);
			return 0;
		}
		if (iMsg == MENU_KILL_ALL_CLIENTS_MSG)
		{
			// Kill all connected clients
			_this->m_server->KillAuthClients();
			return 0;
		}
    if (iMsg == fileTransferDownloadMessage) 
    {
      // WIN32S: retained as an inert fallback - the download pump is now driven
      // by vncClient::PumpIdle(), which removed the FindWindow + checkPointer
      // round trip after it was observed to deliver exactly one pump per
      // download and then go silent.  Nothing posts this message any more; if
      // one ever arrives (stale post), the checkPointer guard still applies.
      vncClient *cl = (vncClient *) wParam;
      vnclog.Print(LL_INTINFO, VNCLOG("fileTransferDownloadMessage received, cl=%p\n"), cl);
      if (_this->m_server->checkPointer(cl)) cl->SendFileDownloadPortion();
      else vnclog.Print(LL_INTERR, VNCLOG("fileTransferDownloadMessage: checkPointer failed, cl=%p auth=%d\n"), cl, (int)_this->m_server->AuthClientCount());
      return 0;
    }

	}
	// Message not recognised
	return DefWindowProc(hwnd, iMsg, wParam, lParam);
}
