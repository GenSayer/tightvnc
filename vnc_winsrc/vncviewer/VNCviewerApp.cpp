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

#include "stdhdrs.h"
#include "vncviewer.h"
#include "VNCviewerApp.h"
#include "Exception.h"

// For WinCE Palm, you might want to use this for debugging, since it
// seems impossible to give the command some arguments.
// #define PALM_LOG 1

VNCviewerApp *pApp;

VNCviewerApp::VNCviewerApp(HINSTANCE hInstance, LPTSTR szCmdLine) {
	pApp = this;
	m_instance = hInstance;

	// Read the command line
	m_options.SetFromCommandLine(szCmdLine);
	
	// Logging info
	vnclog.SetLevel(m_options.m_logLevel);
	if (m_options.m_logToConsole) {
		vnclog.SetMode(Log::ToConsole | Log::ToDebug);
	}
	if (m_options.m_logToFile) {
		vnclog.SetFile(m_options.m_logFilename);
	}

#ifdef PALM_LOG
	// Hack override for WinCE Palm developers who can't give
	// options to commands, not even via shortcuts.
	vnclog.SetLevel(20);
 	vnclog.SetFile(_T("\\Temp\\log"));
#endif
 	
	// Clear connection list
	for (int i = 0; i < MAX_CONNECTIONS; i++)
		m_clilist[i] = NULL;

	// Initialise winsock.
	//
	// WIN32S: must request 1.1, not 2.0.  Win32s ships WinSock 1.1
	// (winsock.dll / the stack vendor's WINSOCK.DLL under Windows 3.1) and
	// wsock32.lib is a 1.1 import library.  Asking for MAKEWORD(2,0) makes
	// WSAStartup fail with WSAVERNOTSUPPORTED on Win32s and on plain Win95
	// without the WinSock 2 update; every socket call after that fails.
	//
	// Also note the original bug: on failure it showed a message box and
	// called PostQuitMessage() but then carried on constructing the app and
	// opening a connection with an uninitialised socket library.  We now
	// record the failure so callers can stop.
	WORD wVersionRequested = MAKEWORD(1, 1);
	WSADATA wsaData;
	m_winsockOK = true;
	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		m_winsockOK = false;
		MessageBox(NULL, _T("Error initialising sockets library"), _T("VNC info"), MB_OK | MB_ICONSTOP);
		PostQuitMessage(1);
		return;
	}
	vnclog.Print(3, _T("Started and Winsock (v %d.%d) initialised\n"),
				 LOBYTE(wsaData.wVersion), HIBYTE(wsaData.wVersion));
}


// The list of clients should fill up from the start and have NULLs
// afterwards.  If the first entry is a NULL, the list is empty.
// (Single-threaded: the omni_mutex_lock that used to guard these is gone.)

void VNCviewerApp::RegisterConnection(ClientConnection *pConn) {
	int i;
	for (i = 0; i < MAX_CONNECTIONS; i++) {
		if (m_clilist[i] == NULL) {
			m_clilist[i] = pConn;
			vnclog.Print(4,_T("Registered connection with app\n"));
			return;
		}
	}
	// If we've got here, something is wrong.
	vnclog.Print(-1, _T("Client list overflow!\n"));
	MessageBox(NULL, _T("Client list overflow!"), _T("VNC error"),
		MB_OK | MB_ICONSTOP);
	PostQuitMessage(1);

}

void VNCviewerApp::DeregisterConnection(ClientConnection *pConn) {
	int i;
	for (i = 0; i < MAX_CONNECTIONS; i++) {
		if (m_clilist[i] == pConn) {
			// shuffle everything above downwards
			for (int j = i; m_clilist[j] &&	j < MAX_CONNECTIONS-1 ; j++)
				m_clilist[j] = m_clilist[j+1];
			m_clilist[MAX_CONNECTIONS-1] = NULL;
			vnclog.Print(4,_T("Deregistered connection from app\n"));

			// No clients left? then we should finish, unless we're in
			// listening mode.
			if ((m_clilist[0] == NULL) && (!pApp->m_options.m_listening)){
				PostQuitMessage(0);}

			return;
		}
	}
	// If we've got here, something is wrong.
	vnclog.Print(-1, _T("Client not found for deregistering!\n"));
	PostQuitMessage(1);
}

// ----------------------------------------------


VNCviewerApp::~VNCviewerApp() {

	// Clean up winsock.
	// Only if WSAStartup actually succeeded: WSACleanup without a matching
	// startup returns WSANOTINITIALISED on NT but is not defined behaviour on
	// every WinSock 1.1 stack.  WinMain also calls WSACleanup; the counts
	// balance because there is exactly one successful WSAStartup.
	if (m_winsockOK)
		WSACleanup();
	
	vnclog.Print(2, _T("VNC viewer closing down\n"));

}
