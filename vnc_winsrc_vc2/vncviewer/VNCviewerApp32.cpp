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

#include "VNCviewerApp32.h"
#include "vncviewer.h"
#include "Exception.h"
#include "ClientConnection.h"
#include "Win32sApi.h"

// --------------------------------------------------------------------------
VNCviewerApp32::VNCviewerApp32(HINSTANCE hInstance, PSTR szCmdLine) :
	VNCviewerApp(hInstance, szCmdLine)
{

	m_pdaemon = NULL;

	// Modeless dialog table must start empty.  This was missing: m_dialogs[]
	// was left holding whatever was on the heap, so ProcessDialogMessage()
	// called IsDialogMessage() on garbage HWNDs on the very first message.
	// Harmless-ish on NT (invalid handle -> FALSE), fatal on Win32s.
	for (int d = 0; d < MAX_MODELESS_DIALOGS; d++)
		m_dialogs[d] = NULL;

	// Load a requested keyboard layout.
	//
	// WIN32S: LoadKeyboardLayout() is a Win95/NT API and is not present in the
	// Win32s USER32 stub set.  It is resolved dynamically (see Win32sApi) so
	// that its mere presence in the import table cannot stop the EXE loading.
	if (m_options.m_kbdSpecified) {
		HKL hkl = Win32sLoadKeyboardLayout(m_options.m_kbdname,
			KLF_ACTIVATE | KLF_REPLACELANG | KLF_REORDER);
		if (hkl == NULL) {
			// Not fatal: on Win32s there is only one layout anyway.  Warn and
			// continue rather than exit(1), which used to kill the viewer
			// before any window existed.
			vnclog.Print(0, _T("Could not load keyboard layout %s (ignored)\n"),
						 m_options.m_kbdname);
		}
	}

	// Start listening daemons if requested
	
	if ((m_options.m_listening) && (FindWindow("VNCviewer Daemon", 0) == NULL)) {
		vnclog.Print(3, _T("In listening mode - staring daemons\n"));
		ListenMode();
	} else {
		m_options.m_listening = false;
	}

	RegisterSounds();
}

	
// These should maintain a list of connections.
// FIXME: Eliminate duplicated code, see the following three functions.

// --------------------------------------------------------------------------
// Connection creation.
//
// WIN32S NOTES on these three functions:
//
//  * The retry loops all had the same bug: on AuthException they created the
//    replacement ClientConnection *before* deleting the old one.  Two live
//    connection objects at once means two registrations in m_clilist, two
//    framebuffer bitmaps and two network buffers - on Win32s, where GDI and
//    memory are scarce, the second allocation is the one that fails.  The
//    order is now: copy the options out, delete the old object, then create
//    the new one.
//
//  * The socket-accepting overload re-created the connection from the *same*
//    SOCKET after an auth failure.  ~ClientConnection closes that socket, so
//    the retry ran on a closed handle.  Reverse connections now get one
//    attempt, which is the only thing that can work.
//
//  * "catch (Exception &e)" did not catch everything it should have: the base
//    Exception::Report() was assert(false) (see Exception.cpp).  That is fixed
//    separately; here we simply make sure no exception escapes into WinMain,
//    which on Win32s would take the process down without a message.
// --------------------------------------------------------------------------

void VNCviewerApp32::NewConnection() {
	int retries = 0;
	ClientConnection *pcc = new ClientConnection(this);
	if (pcc == NULL)
		return;

	while (retries < MAX_AUTH_RETRIES) {
		try {
			pcc->Run();
			return;					// success: the object lives on, driven by
									// PumpConnections() from the main loop
		} catch (AuthException &e) {
			e.Report();
			pcc->UnloadConnection();

			// Save the options, then destroy the old connection *before*
			// building the new one - see the note above.
			VNCOptions savedOpts = pcc->GetOptions();
			delete pcc;
			pcc = NULL;

			pcc = new ClientConnection(this);
			if (pcc == NULL)
				return;
			pcc->SetOptions(savedOpts);
		} catch (Exception &e) {
			e.Report();
			break;
		}
		retries++;
	}
	if (pcc != NULL)
		delete pcc;
}

void VNCviewerApp32::NewConnection(TCHAR *host, int port) {
	int retries = 0;
	ClientConnection *pcc = new ClientConnection(this, host, port);
	if (pcc == NULL)
		return;

	while (retries < MAX_AUTH_RETRIES) {
		try {
			pcc->Run();
			return;
		} catch (AuthException &e) {
			e.Report();

			VNCOptions savedOpts = pcc->GetOptions();
			delete pcc;
			pcc = NULL;

			pcc = new ClientConnection(this, host, port);
			if (pcc == NULL)
				return;
			pcc->SetOptions(savedOpts);
		} catch (Exception &e) {
			e.Report();	
			break;
		}
		retries++;
	}
	if (pcc != NULL)
		delete pcc;
}

void VNCviewerApp32::NewConnection(SOCKET sock) {
	// Reverse (listening-mode) connection.  No retry loop: the socket belongs
	// to this connection and is closed by its destructor, so a second attempt
	// would run on a closed socket.  The old code did exactly that.
	ClientConnection *pcc = new ClientConnection(this, sock);
	if (pcc == NULL) {
		closesocket(sock);
		return;
	}

	try {
		pcc->Run();
		return;
	} catch (AuthException &e) {
		e.Report();
	} catch (Exception &e) {
		e.Report();
	}

	delete pcc;
}

void VNCviewerApp32::ListenMode() {

	try {
		m_pdaemon = new Daemon(m_options.m_listenPort);
	} catch (WarningException &e) {
		char msg[1024];
		// _snprintf, not sprintf: e.m_info is attacker/server-influenced text
		// in other paths and this is a 1 KB stack buffer.
		_snprintf(msg, sizeof(msg) - 1,
				"Error creating listening daemon:\n\r(%s)\n\r%s",
				(e.m_info != NULL) ? e.m_info : "unknown error",
				"Perhaps another VNCviewer is already running?");
		msg[sizeof(msg) - 1] = '\0';
		MessageBox(NULL, msg, "VNCviewer error", MB_OK | MB_ICONSTOP);

		// Was exit(1).  exit() from here runs atexit handlers and static
		// destructors while the app object is only partly constructed; on
		// Win32s that is a reliable way to fault on the way out.  Just turn
		// listening mode off and let WinMain proceed - it will quit normally
		// because there are no connections.
		m_pdaemon = NULL;
		m_options.m_listening = false;
	}
}

// Register the Bell sound event

const char* BELL_APPL_KEY_NAME  = "AppEvents\\Schemes\\Apps\\VNCviewer";
const char* BELL_LABEL = "VNCviewerBell";

void VNCviewerApp32::RegisterSounds() {
	
	// The whole point of these keys is the AppEvents sound scheme, which
	// Windows 3.1 does not have - and the viewer no longer uses PlaySound at
	// all (see Win32sPlayBell in Win32sApi.cpp).  Writing the keys on Win32s
	// achieves nothing and needlessly touches a registry that may be
	// read-only or absent, so skip it there.
	if (Win32sIsWin32s())
		return;

	HKEY hBellKey;
	char keybuf[256];
	
	sprintf(keybuf, "AppEvents\\EventLabels\\%s", BELL_LABEL);
	// First create a label for it
	if ( RegCreateKey(HKEY_CURRENT_USER, keybuf, &hBellKey)  == ERROR_SUCCESS ) {
		RegSetValue(hBellKey, NULL, REG_SZ, "Bell", 0);
		RegCloseKey(hBellKey);
		
		// Then put the detail in the app-specific area
		
		if ( RegCreateKey(HKEY_CURRENT_USER, BELL_APPL_KEY_NAME, &hBellKey)  == ERROR_SUCCESS ) {
			
			// NOTE: the original code leaked/overwrote hBellKey here - it
			// called RegCreateKey again into the same variable without closing
			// the handle from the enclosing "if", and then closed only the
			// inner one.  Each RegCreateKey below is now paired with its close.
			RegCloseKey(hBellKey);

			sprintf(keybuf, "%s\\%s", BELL_APPL_KEY_NAME, BELL_LABEL);
			if (RegCreateKey(HKEY_CURRENT_USER, keybuf, &hBellKey) == ERROR_SUCCESS) {
				RegSetValue(hBellKey, NULL, REG_SZ, "Bell", 0);
				RegCloseKey(hBellKey);
			}
			
			sprintf(keybuf, "%s\\%s\\.current", BELL_APPL_KEY_NAME, BELL_LABEL);
			if (RegOpenKey(HKEY_CURRENT_USER, keybuf, &hBellKey) != ERROR_SUCCESS) {
				if (RegCreateKey(HKEY_CURRENT_USER, keybuf, &hBellKey) == ERROR_SUCCESS) {
					RegSetValue(hBellKey, NULL, REG_SZ, "ding.wav", 0);
					RegCloseKey(hBellKey);
				}
			} else {
				RegCloseKey(hBellKey);
			}
			
			sprintf(keybuf, "%s\\%s\\.default", BELL_APPL_KEY_NAME, BELL_LABEL);
			if (RegOpenKey(HKEY_CURRENT_USER, keybuf, &hBellKey) != ERROR_SUCCESS) {
				if (RegCreateKey(HKEY_CURRENT_USER, keybuf, &hBellKey) == ERROR_SUCCESS) {
					RegSetValue(hBellKey, NULL, REG_SZ, "ding.wav", 0);
					RegCloseKey(hBellKey);
				}
			} else {
				RegCloseKey(hBellKey);
			}
		}
		
	} 
	
}

/* Original STL/list-based version, retained for reference only:

bool VNCviewerApp32::ProcessDialogMessage(MSG *pmsg)
{
	if (!m_dialogs.empty()) {
		omni_mutex_lock l(m_dialogsMutex);
		list<HWND>::iterator iter;
		for (iter = m_dialogs.begin(); iter != m_dialogs.end(); iter++) {
			if (IsDialogMessage(*iter, pmsg))
				return true;
		}
	}
	return false;
} */

bool VNCviewerApp32::ProcessDialogMessage(MSG *pmsg) {
    for (int i = 0; i < MAX_MODELESS_DIALOGS; i++) {
        if (m_dialogs[i] != NULL) {
            if (::IsDialogMessage(m_dialogs[i], pmsg)) {
                return true;
            }
        }
    }
    return false;
}

//
// PumpConnections - idle-time driver for all live connections.
//
// This is the single-threaded replacement for the per-connection worker
// threads.  Called from the idle branch of the main message loop in WinMain.
//
// It walks the connection list and lets each connection service at most one
// pending server message.  A connection only does work if select() says data
// is waiting, so this returns quickly when the network is quiet.
//
// Returns true if any connection processed a message.  WinMain uses that to
// decide whether to keep pumping (busy) or to block in GetMessage (idle).
//
bool VNCviewerApp32::PumpConnections()
{
	bool didWork = false;

	for (int i = 0; i < MAX_CONNECTIONS; i++) {
		ClientConnection *pcc = m_clilist[i];
		if (pcc == NULL)
			break;					// list is packed; NULL means end
		if (pcc->IsDead())
			continue;
		if (pcc->PumpIdle())
			didWork = true;
	}

	return didWork;
}

//
// ReapDeadConnections - delete connections that finished.
//
// A connection flags itself dead in its WM_DESTROY handler
// (ClientConnection::OnWindowDestroyed).  We must not delete it there: that
// would free the object while Windows is still dispatching to its window
// procedure.  Instead the main loop calls this, where the stack is clean.
//
// Note that ~ClientConnection calls DeregisterConnection(), which compacts
// m_clilist, so the index is deliberately not incremented after a delete.
//
void VNCviewerApp32::ReapDeadConnections()
{
	int i = 0;
	while (i < MAX_CONNECTIONS) {
		ClientConnection *pcc = m_clilist[i];
		if (pcc == NULL)
			break;
		if (pcc->IsDead()) {
			delete pcc;				// removes itself from m_clilist
			continue;				// same index now holds the next entry
		}
		i++;
	}
}


VNCviewerApp32::~VNCviewerApp32() {
	// Delete any connections still around, then the listening daemon.
	ReapDeadConnections();
	if (m_pdaemon != NULL) delete m_pdaemon;
}
	
