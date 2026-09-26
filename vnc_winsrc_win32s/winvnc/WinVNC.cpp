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


// 24/11/97		WEZ

// WinMain and main WndProc for the new version of WinVNC

////////////////////////////
// System headers
#include "stdhdrs.h"

////////////////////////////
// Custom headers
#include "VSocket.h"
#include "WinVNC.h"

#include "vncServer.h"
#include "vncMenu.h"
#include "vncInstHandler.h"
#include "vncService.h"
#include "Win32sApi.h"

extern "C" {
#include "ParseHost.h"
}

// Application instance and name
HINSTANCE	hAppInstance;
const char	*szAppName = "WinVNC";

DWORD		mainthreadId;

// WinMain parses the command line and either calls the main App
// routine or, under NT, the main service routine.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow)
{
#ifdef _DEBUG
	{
		// Get current flag
		int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );

		// Turn on leak-checking bit
		tmpFlag |= _CRTDBG_LEAK_CHECK_DF;

		// Set flag to the new value
		_CrtSetDbgFlag( tmpFlag );
	}
#endif

	// WIN32S: resolve the Win95-and-later APIs before anything else uses them.
	// Must be first: vncMenu's constructor needs Win32sShellNotifyIcon, and
	// several dialogs need Win32sGetWorkArea.
	Win32sApiInit();

	// Save the application instance and main thread id
	hAppInstance = hInstance;
	mainthreadId = GetCurrentThreadId();

	// Initialise the VSocket system
	VSocketSystem socksys;
	if (!socksys.Initialised())
	{
		MessageBox(NULL, "Failed to initialise the socket system", szAppName, MB_OK);
		return 0;
	}
	vnclog.Print(LL_STATE, VNCLOG("sockets initialised\n"));

	// Make the command-line lowercase and parse it
	size_t i;
	for (i = 0; i < strlen(szCmdLine); i++)
	{
		szCmdLine[i] = tolower(szCmdLine[i]);
	}

	BOOL argfound = FALSE;
	char *connectName = NULL;
	int connectPort;
	bool cancelConnect = false;
	bool silent = false;

	for (i = 0; i < strlen(szCmdLine); i++)
	{
		if (szCmdLine[i] <= ' ')
			continue;
		argfound = TRUE;

		// Determine the length of current argument in the command line
		size_t arglen = strcspn(&szCmdLine[i], " \t\r\n\v\f");

		// Now check for command-line arguments
		if (strncmp(&szCmdLine[i], winvncRunServiceHelper, arglen) == 0 &&
			arglen == strlen(winvncRunServiceHelper))
		{
			// NB : This flag MUST be parsed BEFORE "-service", otherwise it will match
			// the wrong option!  (This code should really be replaced with a simple
			// parser machine and parse-table...)

			// Run the WinVNC Service Helper app
			vncService::PostUserHelperMessage();
			return 0;
		}
		if (strncmp(&szCmdLine[i], winvncRunService, arglen) == 0 &&
			arglen == strlen(winvncRunService))
		{
			// Run WinVNC as a service.
			// WIN32S: stubbed - reports that services are unavailable.
			return vncService::WinVNCServiceMain();
		}
		if (strncmp(&szCmdLine[i], winvncRunAsUserApp, arglen) == 0 &&
			arglen == strlen(winvncRunAsUserApp))
		{
			// WinVNC is being run as a user-level program
			return WinVNCAppMain();
		}
		if (strncmp(&szCmdLine[i], winvncInstallService, arglen) == 0 &&
			arglen == strlen(winvncInstallService))
		{
			// Install WinVNC as a service
			vncService::InstallService(silent);
			i += arglen;
			continue;
		}
		if (strncmp(&szCmdLine[i], winvncReinstallService, arglen) == 0 &&
			arglen == strlen(winvncReinstallService))
		{
			// Silently remove WinVNC, then re-install it
			vncService::ReinstallService(silent);
			i += arglen;
			continue;
		}
		if (strncmp(&szCmdLine[i], winvncSilentMode, arglen) == 0 &&
			arglen == strlen(winvncSilentMode))
		{
			silent = true;
			i += arglen;
			continue;
		}
		if (strncmp(&szCmdLine[i], winvncRemoveService, arglen) == 0 &&
			arglen == strlen(winvncRemoveService))
		{
			// Remove the WinVNC service
			vncService::RemoveService(silent);
			i += arglen;
			continue;
		}
		if (strncmp(&szCmdLine[i], winvncReload, arglen) == 0 &&
			arglen == strlen(winvncReload))
		{
			// Reload Properties from the registry
			vncService::PostReloadMessage();
			i += arglen;
			continue;
		}
		if (strncmp(&szCmdLine[i], winvncShowProperties, arglen) == 0 &&
			arglen == strlen(winvncShowProperties))
		{
			// Show the Properties dialog of an existing instance of WinVNC
			vncService::ShowProperties();
			i += arglen;
			continue;
		}
		if (strncmp(&szCmdLine[i], winvncShowDefaultProperties, arglen) == 0 &&
			arglen == strlen(winvncShowDefaultProperties))
		{
			// Show the Properties dialog of an existing instance of WinVNC
			vncService::ShowDefaultProperties();
			i += arglen;
			continue;
		}
		if (strncmp(&szCmdLine[i], winvncShowAbout, arglen) == 0 &&
			arglen == strlen(winvncShowAbout))
		{
			// Show the About dialog of an existing instance of WinVNC
			vncService::ShowAboutBox();
			i += arglen;
			continue;
		}
		if (strncmp(&szCmdLine[i], winvncKillAllClients, arglen) == 0 &&
			arglen == strlen(winvncKillAllClients))
		{
			// NB : This flag MUST be parsed BEFORE "-kill", otherwise it will match
			// the wrong option!

			// Kill all connected clients
			vncService::KillAllClients();
			i += arglen;
			continue;
		}
		if (strncmp(&szCmdLine[i], winvncKillRunningCopy, arglen) == 0 &&
			arglen == strlen(winvncKillRunningCopy))
		{
			// Kill any already running copy of WinVNC
			vncService::KillRunningCopy();
			i += arglen;
			continue;
		}
		if (strncmp(&szCmdLine[i], winvncShareAll, arglen) == 0 &&
			arglen == strlen(winvncShareAll))
		{
			// Show full desktop to VNC clients
			vncService::PostShareAll();
			i += arglen;
			continue;
		}
		if (strncmp(&szCmdLine[i], winvncSharePrimary, arglen) == 0 &&
			arglen == strlen(winvncSharePrimary))
		{
			// Show only the primary display to VNC clients
			vncService::PostSharePrimary();
			i += arglen;
			continue;
		}
		if (strncmp(&szCmdLine[i], winvncShareArea, arglen) == 0 &&
			arglen == strlen(winvncShareArea))
		{
			// Show a specified rectangular area to VNC clients
			i += arglen;

			// First, we have to parse the command line to get an argument
			int start, end;
			start = i;
			while (szCmdLine[start] && szCmdLine[start] <= ' ') start++;
			end = start;
			while (szCmdLine[end] > ' ') end++;
			i = end;
			if (end == start)
				continue;

			// Parse the argument -- it should look like 640x480+320+240
			unsigned short x, y, w, h;
			int n = sscanf(&szCmdLine[start], "%hux%hu+%hu+%hu", &w, &h, &x, &y);
			if (n == 4 && w > 0 && h > 0)
				vncService::PostShareArea(x, y, w, h);

			continue;
		}
		if (strncmp(&szCmdLine[i], winvncShareWindow, arglen) == 0 &&
			arglen == strlen(winvncShareWindow))
		{
			// Find a window to share, by its title
			i += arglen;

			cancelConnect = true;	// Ignore the -connect option unless
									// there will be valid window to share

			int start = i, end;
			while (szCmdLine[start] && szCmdLine[start] <= ' ') start++;
			if (szCmdLine[start] == '"') {
				start++;
				char *ptr = strchr(&szCmdLine[start], '"');
				if (ptr == NULL) {
					end = strlen(szCmdLine);
					i = end;
				} else {
					end = ptr - szCmdLine;
					i = end + 1;
				}
			} else {
				end = start;
				while (szCmdLine[end] > ' ') end++;
				i = end;
			}
			if (end - start > 0) {
				char *title = new char[end - start + 1];
				if (title != NULL) {
					strncpy(title, &szCmdLine[start], end - start);
					title[end - start] = 0;
					HWND hwndFound = vncService::FindWindowByTitle(title);
					if (hwndFound != NULL)
						cancelConnect = false;
					vncService::PostShareWindow(hwndFound);
					delete [] title;
				}
			}
			continue;
		}
		if (strncmp(&szCmdLine[i], winvncAddNewClient, arglen) == 0 &&
			arglen == strlen(winvncAddNewClient) && connectName == NULL)
		{
			// Add a new client to an existing copy of winvnc
			i += arglen;

			// First, we have to parse the command line to get the hostname to use
			int start, end;
			start=i;
			while (szCmdLine[start] && szCmdLine[start] <= ' ') start++;
			end = start;
			while (szCmdLine[end] > ' ') end++;

			connectName = new char[end-start+1];

			// Was there a hostname (and optionally a port number) given?
			if (end-start > 0) {
				if (connectName != NULL) {
					strncpy(connectName, &(szCmdLine[start]), end-start);
					connectName[end-start] = 0;
					connectPort = ParseHostPort(connectName, INCOMING_PORT_OFFSET);
				}
			} else {
				if (connectName != NULL)
					connectName[0] = '\0';
			}
			i = end;
			continue;
		}

		// Either the user gave the -help option or there is something odd on the cmd-line!

		// Show the usage dialog
		MessageBox(NULL, winvncUsageText, "WinVNC Usage", MB_OK | MB_ICONINFORMATION);
		break;
	}

	// If no arguments were given then just run
	if (!argfound)
		return WinVNCAppMain();

	// Process the -connect option at the end
	if (connectName != NULL && !cancelConnect) {
		if (connectName[0] != '\0') {
			VCard32 address = VSocket::Resolve(connectName);
			if (address != 0) {
				// Post the IP address to the server
				vncService::PostAddNewClient(address, connectPort);
			}
		} else {
			// Tell the server to show the Add New Client dialog
			vncService::PostAddNewClient(0, 0);
		}
	}
	if (connectName != NULL)
		delete[] connectName;

	return 0;
}

// This is the main routine for WinVNC when running as an application.
//
// ==========================================================================
// WIN32S SINGLE-THREADED MAIN LOOP
//
// The original was a plain GetMessage() loop:
//
//     while (GetMessage(&msg, NULL, 0, 0)) {
//         TranslateMessage(&msg);
//         DispatchMessage(&msg);
//     }
//
// That worked because everything else ran on its own thread: one thread per
// client reading its socket, one for the desktop poll, one blocking in accept()
// on the listening socket, one for the HTTP listener.
//
// With no threads, this loop must also drive all of that.  The structure is the
// same one used by the viewer:
//
//   PeekMessage   - drain every pending window message first, so that the tray
//                   menu, the properties dialogs and repainting always take
//                   priority over network and capture work.
//
//   PumpIdle      - vncServer::PumpIdle() accepts connections, services each
//                   client, polls the screen and reaps finished clients.  See
//                   the long comment on it in vncServer.cpp for the ordering
//                   rationale.
//
//   WaitMessage   - when there is nothing at all to do, yield.  This is
//                   essential on Win32s: scheduling is cooperative, so a busy
//                   loop here freezes every other application on the machine,
//                   including the very Windows 3.1 desktop we are trying to
//                   serve.
//
//   A 50 ms timer makes WaitMessage() safe.  Without it, a client that sends a
//   request while we are idle would not be noticed until some unrelated message
//   arrived.  The timer is a NULL-window timer, so its WM_TIMER simply wakes the
//   loop and is then discarded by DispatchMessage.
//
// Note the deliberate asymmetry with the viewer: the viewer pumps up to 16
// network messages per pass, because it is decoding a screen and throughput
// matters.  Here PumpIdle() is called once per pass, because its third step is a
// screen capture - doing that repeatedly without returning to the message queue
// would make the local machine unusable.
// ==========================================================================

int WinVNCAppMain()
{
	// WIN32S: SetProcessShutdownParameters is a Win95/NT API.  It is not in the
	// Win32s KERNEL32 stub set, and because the linker records it as an import
	// the EXE would not LOAD on Win32s at all - the process would never start and
	// there would be no diagnostic.  It only adjusts shutdown ORDER, which is
	// meaningless on a system with no orderly shutdown protocol, so it is simply
	// dropped rather than resolved dynamically.
	//
	// Was:  SetProcessShutdownParameters(0x100, 0);

	// Check for previous instances of WinVNC!
	vncInstHandler instancehan;
	if (!instancehan.Init())
	{
		// We don't allow multiple instances!
		MessageBox(NULL, "Another instance of WinVNC is already running", szAppName, MB_OK);
		return 0;
	}

	// CREATE SERVER
	vncServer server;

	// Set the name and port number
	server.SetName(szAppName);
	vnclog.Print(LL_STATE, VNCLOG("server created ok\n"));

	// Create tray icon & menu if we're running as an app.
	//
	// WIN32S: there is no system tray on Windows 3.1.  vncMenu detects that and
	// shows an ordinary (minimised) window instead, so that the user still has
	// somewhere to click for the menu and a way to quit.  See vncMenu.cpp.
	vncMenu *menu = new vncMenu(&server);
	if (menu == NULL)
	{
		vnclog.Print(LL_INTERR, VNCLOG("failed to create tray menu\n"));
		PostQuitMessage(0);
	}

	// ----------------------------------------------------------------------
	// Main loop.  See the long comment above.
	// ----------------------------------------------------------------------

	const UINT IDLE_POLL_TIMER = 0x7FF2;
	const UINT IDLE_POLL_MS    = 50;

	// SetTimer returns UINT in the MSVC 4.1 SDK, not UINT_PTR.
	UINT pollTimer = SetTimer(NULL, IDLE_POLL_TIMER, IDLE_POLL_MS, NULL);
	if (pollTimer == 0) {
		// Windows 3.1 has a small system-wide timer limit and this can genuinely
		// fail.  Without the timer the loop still works, but it will only notice
		// network activity when some other message arrives - so warn loudly.
		vnclog.Print(LL_INTERR,
			VNCLOG("could not create idle timer - responsiveness will suffer\n"));
	}

	MSG msg;
	memset(&msg, 0, sizeof(msg));	// so the final "return msg.wParam" is defined
									// even if the loop never runs
	BOOL quit = FALSE;

	while (!quit)
	{
		// 1. Drain the message queue.
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT) {
				quit = TRUE;
				break;
			}
			vnclog.Print(LL_INTINFO, VNCLOG("message %d received\n"), msg.message);
			TranslateMessage(&msg);	// convert key ups and downs to chars
			DispatchMessage(&msg);
		}
		if (quit)
			break;

		// 2. Drive the server: accept connections, service clients, poll the
		//    screen, reap finished clients.
		BOOL busy = server.PumpIdle();

		// 3. Nothing to do?  Yield until the next message or timer tick.
		if (!busy)
		{
			if (!PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
				WaitMessage();
		}
	}

	if (pollTimer != 0)
		KillTimer(NULL, pollTimer);

	vnclog.Print(LL_STATE, VNCLOG("shutting down server\n"));

	if (menu != NULL)
		delete menu;

	return msg.wParam;
}
