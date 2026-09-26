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


// vncSockConnect.cpp

// Implementation of the listening socket class

// ==========================================================================
// WIN32S SINGLE-THREADED CONVERSION
//
// "class vncSockConnectThread : public omni_thread" used to live here.  Its
// run_undetached() sat in a loop calling TryAccept(&new_socket, 100), i.e. a
// blocking accept with a 100 ms timeout, and handed each accepted socket to
// vncServer::AddClient().
//
// Win32s has no threads, so there are two possible replacements:
//
//   (a) WSAAsyncSelect(sock, hwnd, msg, FD_ACCEPT) - the stack posts a message
//       to a window when a connection arrives.  This is the idiomatic WinSock
//       1.1 approach and is what the VIEWER's listening daemon uses.
//
//   (b) Poll the listening socket from the application's idle loop with a
//       zero-timeout select().
//
// This file uses (b), deliberately, and it is worth recording why:
//
//   * WSAAsyncSelect requires a window to post to.  vncSockConnect has no window
//     of its own; it would have to borrow the vncMenu window, which means adding
//     a message ID to vncMenu's already large WndProc and coupling the listener
//     to the tray UI.  On Win32s the tray window may also be shown as a real
//     window (there is no system tray), so its message handling is already
//     doing more than it does on Win9x.
//
//   * WSAAsyncSelect(FD_ACCEPT) puts the LISTENING socket into non-blocking
//     mode, and on several Windows 3.1 stacks that also makes the socket
//     returned by accept() non-blocking.  vncClient's handshake
//     (RunHandshake -> VSocket::ReadExact) expects a blocking socket, so each
//     accepted socket would need ioctlsocket(FIONBIO, 0) applied to it - which
//     the viewer's Daemon.cpp does, but which is easy to get wrong and hard to
//     diagnose when it is.
//
//   * The server already has an idle loop that must run anyway, for screen
//     polling.  Adding one zero-timeout select() per pass costs nothing
//     measurable next to the screen capture.
//
// The result: Init() creates and binds the socket exactly as before but starts
// no thread, and PumpIdle() does one non-blocking accept attempt per call.
// ==========================================================================

#include "stdhdrs.h"
#include "VSocket.h"
#include "vncSockConnect.h"
#include "vncServer.h"
#include <omnithread.h>

// The vncSockConnect class implementation

vncSockConnect::vncSockConnect()
{
	m_server = NULL;
	m_port = 0;
	m_shutdown = FALSE;
	m_listening = FALSE;
}

vncSockConnect::~vncSockConnect()
{
	m_shutdown = TRUE;
	m_listening = FALSE;

	// WIN32S: was
	//     m_socket.Shutdown();
	//     if (m_thread != NULL) {
	//         ((vncSockConnectThread *)m_thread)->m_shutdown = TRUE;
	//         void *returnval;
	//         m_thread->join(&returnval);      // wait for the accept loop to exit
	//         m_socket.Close();
	//     }
	//
	// There is no thread to signal or join.  Note that the original only called
	// Close() if a thread had been created - so a vncSockConnect that got as far
	// as Bind() but failed at Listen() leaked its socket.  Always close.
	m_socket.Shutdown();
	m_socket.Close();
}

BOOL vncSockConnect::Init(vncServer *server, UINT port)
{
	// Save the server and port
	m_server = server;
	m_port = port;
	m_shutdown = FALSE;
	m_listening = FALSE;

	if (server == NULL)
		return FALSE;

	// Create the listening socket
	if (!m_socket.Create()) {
		vnclog.Print(LL_INTERR, VNCLOG("failed to create listening socket\n"));
		return FALSE;
	}

	// Bind it
	if (!m_socket.Bind(m_port, server->LoopbackOnly())) {
		vnclog.Print(LL_INTERR,
			VNCLOG("failed to bind listening socket to port %d\n"), (int)m_port);
		m_socket.Close();
		return FALSE;
	}

	// Set it to listen
	if (!m_socket.Listen()) {
		vnclog.Print(LL_INTERR, VNCLOG("failed to listen on port %d\n"), (int)m_port);
		m_socket.Close();
		return FALSE;
	}

	// WIN32S: no thread is started.  The application idle loop calls PumpIdle().
	m_listening = TRUE;
	vnclog.Print(LL_STATE, VNCLOG("listening for connections on port %d\n"),
				 (int)m_port);

	return TRUE;
}

// ==========================================================================
// PumpIdle - accept at most one pending connection, without blocking.
//
// This is the body of the old accept loop, minus the loop.  Called from
// vncServer::PumpIdle(), which the application idle loop calls.
//
// Returns TRUE if a connection was accepted, so the caller knows it did work.
//
// IMPORTANT: only ONE connection is accepted per call.  AddClient() runs the
// whole blocking RFB handshake inline (see vncClient::Init), so accepting a
// backlog of connections in a single pass would stall the server - and the
// screen polling with it - for as long as all those handshakes take.
// ==========================================================================
BOOL vncSockConnect::PumpIdle()
{
	if (m_shutdown || !m_listening)
		return FALSE;

	// Zero timeout: return immediately if nothing is waiting.  The original used
	// 100 ms, which was fine on its own thread but would cost the single thread
	// 100 ms of every idle pass here.
	VSocket *new_socket = NULL;
	if (!m_socket.TryAccept(&new_socket, 0)) {
		// A genuine socket error on the listening socket.  The old thread broke
		// out of its loop and exited, silently ending listening mode; say so.
		vnclog.Print(LL_INTERR,
			VNCLOG("error on listening socket - no longer accepting connections\n"));
		m_listening = FALSE;
		return FALSE;
	}

	if (new_socket == NULL)
		return FALSE;					// nothing waiting

	vnclog.Print(LL_CLIENTS, VNCLOG("accepted connection from %s\n"),
				 new_socket->GetPeerName());

	// Successful accept - start the client unauthenticated.
	//
	// AddClient takes ownership of new_socket: on success the vncClient owns it,
	// and on failure AddClient deletes it.  Do not delete it here.
	m_server->AddClient(new_socket, FALSE, FALSE);

	return TRUE;
}
