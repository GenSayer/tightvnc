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
#include "Exception.h"
#include "Win32sApi.h"
#ifdef UNDER_CE
#include "omnithreadce.h"
#else
#include "omnithread.h"
#include "VNCviewerApp32.h"
#endif


// All logging is done via the log object
Log vnclog;
VNCHelp help;
HotKeys hotkeys;

#ifdef UNDER_CE
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR szCmdLine, int iCmdShow)
#else
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow)
#endif
{
	// Resolve the Win95+ APIs we use before anything else touches them.
	// Must be first: VNCviewerApp32's constructor already needs
	// Win32sLoadKeyboardLayout, and Log/CentreWindow need Win32sGetWorkArea.
	Win32sApiInit();

	// The state of the application as a whole is contained in the one app object
	#ifdef _WIN32_WCE
		VNCviewerApp app(hInstance, szCmdLine);
	#else
		VNCviewerApp32 app(hInstance, szCmdLine);
	#endif

	// If WSAStartup failed there is no point continuing: every socket call
	// would fail and the old code carried on regardless.
	if (!app.m_winsockOK) {
		vnclog.Print(0, _T("Sockets library unavailable - exiting\n"));
		return 1;
	}

	// Start a new connection if specified on command line, 
	// or if not in listening mode
	
	if (app.m_options.m_connectionSpecified) {
		app.NewConnection(app.m_options.m_host, app.m_options.m_port);
	} else if (!app.m_options.m_listening) {
		// This one will also read from config file if specified
		app.NewConnection();
	}

	// ----------------------------------------------------------------------
	// Main loop.
	//
	// WIN32S SINGLE-THREADED DESIGN
	//
	// The original loop was a plain GetMessage() loop, because each connection
	// had its own thread reading the socket.  With one thread the loop must
	// also (a) service sockets and (b) delete finished connections.
	//
	//   PeekMessage  - drain all pending window messages first, so input and
	//                  painting always win over network work.
	//   PumpConnections - each live connection reads at most one server
	//                  message, and only if select() says data is waiting.
	//   ReapDeadConnections - free connections whose window has been
	//                  destroyed.  Done here, never inside a window
	//                  procedure, so we cannot delete an object Windows is
	//                  still dispatching to.
	//   WaitMessage  - when there is nothing to draw and nothing to read we
	//                  must not spin: on Win32s/Windows 3.1 scheduling is
	//                  cooperative, so a busy loop starves every other
	//                  application on the machine.  WaitMessage() yields.
	//                  A 50 ms timer (below) guarantees we wake up again to
	//                  re-poll the socket even if no message ever arrives.
	//
	// The timer is what makes WaitMessage() safe: without it, a server that
	// sends an update while we are idle would not be noticed until the user
	// moved the mouse.
	// ----------------------------------------------------------------------

	const UINT IDLE_POLL_TIMER = 0x7FF1;
	const UINT IDLE_POLL_MS    = 50;

	// A NULL-window timer posts WM_TIMER to the message queue, which both
	// wakes WaitMessage() and is harmlessly ignored by the dispatch below.
	// SetTimer returns UINT in the MSVC 4.1 SDK (not UINT_PTR).
	UINT pollTimer = SetTimer(NULL, IDLE_POLL_TIMER, IDLE_POLL_MS, NULL);

	MSG msg;
	memset(&msg, 0, sizeof(msg));	// so the final "return msg.wParam" is
									// defined even if we never loop
	bool quit = false;

	try {
		while (!quit) {

			// 1. Drain the message queue.
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
				if (msg.message == WM_QUIT) {
					quit = true;
					break;
				}
				if ( !hotkeys.TranslateAccel(&msg) &&
					 !help.TranslateMsg(&msg) &&
					 !app.ProcessDialogMessage(&msg) ) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
			if (quit)
				break;

			// 2. Free anything that finished while we were dispatching.
			app.ReapDeadConnections();

			// 3. Give the connections a chance to read from the network.
			//
			//    Service several messages per pass, not one.  With one message
			//    per pass the loop went back through PeekMessage and
			//    WaitMessage for every single framebuffer rectangle, and on
			//    Win32s that message-queue round trip costs more than the
			//    decode does - which is why loading a screen "takes forever".
			//    The cap keeps the UI responsive: we always return to the
			//    message queue after at most MAX_PUMP_PER_PASS messages.
			bool busy = false;
			{
				const int MAX_PUMP_PER_PASS = 16;
				for (int np = 0; np < MAX_PUMP_PER_PASS; np++) {
					if (!app.PumpConnections())
						break;
					busy = true;
					// Stop early if the user did something: input latency
					// matters more than throughput.
					if (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_NOREMOVE) ||
						PeekMessage(&msg, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_NOREMOVE))
						break;
				}
			}

			// 4. Nothing to do?  Yield to Windows until the next message or
			//    timer tick.  Do not Sleep() and do not spin.
			if (!busy) {
				if (!PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
					WaitMessage();
				}
			}
		}
	} catch (WarningException &e) {
		e.Report();
	} catch (QuietException &e) {
		e.Report();
	}

	if (pollTimer != 0)
		KillTimer(NULL, pollTimer);

	// Delete any surviving connections before winsock goes away.
	app.ReapDeadConnections();

	vnclog.Print(3, _T("Exiting\n"));

	// NOTE: WSACleanup() is NOT called here any more.  VNCviewerApp's
	// destructor does it, and it runs after this function returns.  The old
	// code called WSACleanup() here *and* in the destructor, i.e. once more
	// than WSAStartup succeeded.
	return msg.wParam;
}


// Move the given window to the centre of the screen
// and bring it to the top.
void CentreWindow(HWND hwnd)
{
	RECT winrect, workrect;
	
	// Find how large the desktop work area is.
	// Win32sGetWorkArea always fills workrect: on Win32s it is the whole
	// screen (no taskbar exists), elsewhere it is the real work area.
	// The old code called SystemParametersInfo on an uninitialised RECT and
	// used the result unconditionally.
	Win32sGetWorkArea(&workrect);
	int workwidth = workrect.right -  workrect.left;
	int workheight = workrect.bottom - workrect.top;
	
	// And how big the window is
	GetWindowRect(hwnd, &winrect);
	int winwidth = winrect.right - winrect.left;
	int winheight = winrect.bottom - winrect.top;
	// Make sure it's not bigger than the work area
	winwidth = min(winwidth, workwidth);
	winheight = min(winheight, workheight);

	// Now centre it
	SetWindowPos(hwnd, 
		HWND_TOP,
		workrect.left + (workwidth-winwidth) / 2,
		workrect.top + (workheight-winheight) / 2,
		winwidth, winheight, 
		SWP_SHOWWINDOW);
	Win32sSetForegroundWindow(hwnd);
}

// Convert "host:display" or "host::port" into host and port
// Returns true if valid format, false if not.
// Takes initial string, addresses of results and size of host buffer in wchars.
// If the display info passed in is longer than the size of the host buffer, it
// is assumed to be invalid, so false is returned.
// If the function returns true, then it also replaces the display[]
// string with its canonical representation.
bool ParseDisplay(LPTSTR display, LPTSTR phost, int hostlen, int *pport) 
{
    if (hostlen < (int)_tcslen(display))
        return false;

    int tmp_port;
    // Was L':' / L'\0': wide-character literals in an ANSI (_MBCS) build.
    // They happen to have the right numeric value, but the types are wrong and
    // MSVC 4.1 warns; use the _T() form so this is correct either way.
    TCHAR *colonpos = _tcschr(display, _T(':'));
    if (colonpos == NULL) {
		// No colon -- use default port number
        tmp_port = RFB_PORT_OFFSET;
		_tcsncpy(phost, display, MAX_HOST_NAME_LEN - 1);
		phost[MAX_HOST_NAME_LEN - 1] = _T('\0');
	} else {
		// Guard against a host part longer than the caller's buffer.
		int hostChars = colonpos - display;
		if (hostChars > hostlen - 1)
			return false;
		_tcsncpy(phost, display, hostChars);
		phost[hostChars] = _T('\0');
		if (colonpos[1] == _T(':')) {
			// Two colons -- interpret as a port number
			if (_stscanf(colonpos + 2, TEXT("%d"), &tmp_port) != 1) 
				return false;
		} else {
			// One colon -- interpret as a display or port number
			if (_stscanf(colonpos + 1, TEXT("%d"), &tmp_port) != 1) 
				return false;
			if (tmp_port < 100)
				tmp_port += RFB_PORT_OFFSET;
		}
	}
    *pport = tmp_port;

	// FIXME: We should not overwrite display[] here, buffer overflow
	// is possible.
	
	FormatDisplay(tmp_port, display, phost);
    return true;
}

void FormatDisplay(int port, LPTSTR display, LPTSTR host)
{
	if (port == 5900) {
		_tcscpy(display, host);
	} else if (port > 5900 && port <= 5999) {
			_stprintf(display, TEXT("%s:%d"), host, port - 5900);
	} else {
		_stprintf(display, TEXT("%s::%d"), host, port);
	}
}