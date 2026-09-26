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

#include "VNCviewerApp.h"
#include "Daemon.h"
//#include "list.h"

// Define a maximum ceiling for simultaneous modeless dialogs
#define MAX_MODELESS_DIALOGS 16

class VNCviewerApp32 : public VNCviewerApp {
public:
	VNCviewerApp32(HINSTANCE hInstance, PSTR szCmdLine);
	void ListenMode();
	void NewConnection();
	void NewConnection(TCHAR *host, int port);
	void NewConnection(SOCKET sock);
	Daemon  *m_pdaemon;
	~VNCviewerApp32();
private:
	void RegisterSounds();

public:
	// Clean inline array implementations to replace the broken STL engine.
	// No locking needed: single-threaded (see omnithread/omnithread.h).
	void AddModelessDialog(HWND hwnd) {
		for(int i = 0; i < MAX_MODELESS_DIALOGS; i++) {
			if(m_dialogs[i] == NULL) {
				m_dialogs[i] = hwnd;
				break;
			}
		}
	}
	
	void RemoveModelessDialog(HWND hwnd) {
		for(int i = 0; i < MAX_MODELESS_DIALOGS; i++) {
			if(m_dialogs[i] == hwnd) {
				m_dialogs[i] = NULL;
				break;
			}
		}
	}
	
	bool ProcessDialogMessage(MSG *pmsg);

	// ---- single-threaded session driving (see VNCviewerApp32.cpp) --------

	// Called from the idle path of the main message loop.  Gives every live
	// connection a chance to read one pending server message.  Returns true
	// if any connection did work, so the loop can pump again before blocking.
	bool PumpConnections();

	// Delete connections that flagged themselves dead in their WM_DESTROY.
	// Must only be called from the main loop, never from a window procedure.
	void ReapDeadConnections();

private:
	// Flat array layout completely circumvents MSVC 4.1 template parser bugs
	HWND m_dialogs[MAX_MODELESS_DIALOGS];
};

/* Original STL-based version, kept for reference.  It cannot be used with
   MSVC 4.1 (list<> template) and the mutex is pointless in a single-threaded
   build:

class VNCviewerApp32 : public VNCviewerApp {
public:
	VNCviewerApp32(HINSTANCE hInstance, PSTR szCmdLine);
	void ListenMode();
	void NewConnection();
	void NewConnection(TCHAR *host, int port);
	void NewConnection(SOCKET sock);
	Daemon  *m_pdaemon;
	~VNCviewerApp32();
private:
	// Set up registry for program's sounds
	void RegisterSounds();

// The list of modeless dialogs is maintained for proper message dispatching
public:
	// Functions to operate on the m_dialogs list
	void AddModelessDialog(HWND hwnd) { omni_mutex_lock l(m_dialogsMutex); m_dialogs.push_back(hwnd); }
	void RemoveModelessDialog(HWND hwnd) { omni_mutex_lock l(m_dialogsMutex); m_dialogs.remove(hwnd); }
	bool ProcessDialogMessage(MSG *pmsg);
private:
	// List of open modeless dialogs
	list<HWND> m_dialogs;
	omni_mutex m_dialogsMutex;
}; */

