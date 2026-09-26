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


// vncSockConnect.h

// The vncSockConnect class creates a listening socket and binds
// it to the specified port.  It then creates a listen thread which
// goes into a loop, listening on the socket.
// When the vncSockConnect object is destroyed, all resources are
// freed automatically, including the listen thread.

class vncSockConnect;

#if (!defined(_WINVNC_VNCSOCKCONNECT))
#define _WINVNC_VNCSOCKCONNECT

// Includes
#include "stdhdrs.h"
#include "VSocket.h"
#include "vncServer.h"
#include <omnithread.h>

// The vncSockConnect class itself
//
// WIN32S: this no longer owns a thread.  Init() binds and listens; the
// application idle loop calls PumpIdle(), which performs one non-blocking accept
// attempt.  See the long note at the top of vncSockConnect.cpp for why polling
// was chosen over WSAAsyncSelect here.
class vncSockConnect
{
public:
	// Constructor/destructor
	vncSockConnect();
	~vncSockConnect();

	// Init
	virtual VBool Init(vncServer *server, UINT port);

	// Accept at most one pending connection.  Non-blocking.
	// Returns TRUE if a connection was accepted.
	BOOL PumpIdle();

	// TRUE while the listening socket is usable.
	BOOL IsListening() { return m_listening && !m_shutdown; }

	// Implementation
protected:
	// The listening socket
	VSocket m_socket;

	// The port to listen on
	UINT m_port;

	// The server to hand accepted connections to (was held by the thread).
	vncServer *m_server;

	// Was vncSockConnectThread::m_shutdown.
	BOOL m_shutdown;

	// TRUE once bind+listen have succeeded.
	BOOL m_listening;
};

#endif // _WINVNC_VNCSOCKCONNECT
