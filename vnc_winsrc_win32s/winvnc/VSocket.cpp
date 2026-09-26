//  Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
//  Copyright (C) 2001 HorizonLive.com, Inc. All Rights Reserved.
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


// VSocket.cpp

// The VSocket class provides a platform-independent socket abstraction
// with the simple functionality required for an RFB server.

class VSocket;

////////////////////////////////////////////////////////
// System includes

#include "stdhdrs.h"

// Visual C++ .NET 2003 compatibility
#if (_MSC_VER>= 1300)
#include <iostream>
#else
#include <iostream.h>
#endif

#include <stdio.h>
#ifdef __WIN32__
#include <io.h>
#include <winsock.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#endif
#include <sys/types.h>

////////////////////////////////////////////////////////
// Custom includes

#include "VTypes.h"

////////////////////////////////////////////////////////
// *** Lovely hacks to make Win32 work.  Hurrah!

#ifdef __WIN32__
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif

////////////////////////////////////////////////////////
// Socket implementation

#include "VSocket.h"

// The socket timeout value (currently 5 seconds, for no reason...)
// *** THIS IS NOT CURRENTLY USED ANYWHERE
const VInt rfbMaxClientWait = 5000;

////////////////////////////
// Socket implementation initialisation

static WORD winsockVersion = 0;

VSocketSystem::VSocketSystem()
{
  // Initialise the socket subsystem
  // This is only provided for compatibility with Windows.

#ifdef __WIN32__
  // Initialise WinPoxySockets on Win32
  WORD wVersionRequested;
  WSADATA wsaData;
	
  // WIN32S: must request 1.1, not 2.0.
  //
  // Win32s ships WinSock 1.1 (the stack vendor's WINSOCK.DLL under Windows
  // 3.1), and wsock32.lib is a 1.1 import library.  Asking for MAKEWORD(2,0)
  // makes WSAStartup fail with WSAVERNOTSUPPORTED, m_status becomes VFalse, and
  // WinMain then shows "Failed to initialise the socket system" and exits.
  // This is the same change already made in the viewer.
  wVersionRequested = MAKEWORD(1, 1);
  if (WSAStartup(wVersionRequested, &wsaData) != 0)
  {
    m_status = VFalse;
	return;
  }

  winsockVersion = wsaData.wVersion;
 
#else
  // Disable the nasty read/write failure signals on UNIX
  signal(SIGPIPE, SIG_IGN);
#endif

  // If successful, or if not required, then continue!
  m_status = VTrue;
}

VSocketSystem::~VSocketSystem()
{
	if (m_status)
	{
		WSACleanup();
	}
}

////////////////////////////

VSocket::VSocket()
{
  // Clear out the internal socket fields
  sock = -1;
  out_queue = NULL;
}

////////////////////////////

VSocket::~VSocket()
{
  // Close the socket
  Close();
}

////////////////////////////

VBool
VSocket::Create()
{
  const int one = 1;

  // Check that the old socket was closed
  if (sock >= 0)
    Close();

  // Create the socket
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      return VFalse;
    }

  // Set the socket options:
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one)))
    {
      return VFalse;
    }
  if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof(one)))
	{
	  return VFalse;
	}

  return VTrue;
}

////////////////////////////

VBool
VSocket::Close()
{
  if (sock >= 0)
    {
	  vnclog.Print(LL_SOCKINFO, VNCLOG("closing socket\n"));

	  shutdown(sock, SD_BOTH);
#ifdef __WIN32__
	  closesocket(sock);
#else
	  close(sock);
#endif
      sock = -1;
    }
  while (out_queue)
	{
	  AIOBlock *next = out_queue->next;
	  delete out_queue;
	  out_queue = next;
	}

  return VTrue;
}

////////////////////////////

VBool
VSocket::Shutdown()
{
  if (sock >= 0)
    {
	  vnclog.Print(LL_SOCKINFO, VNCLOG("shutdown socket\n"));

	  shutdown(sock, SD_BOTH);
    }
  while (out_queue)
	{
	  AIOBlock *next = out_queue->next;
	  delete out_queue;
	  out_queue = next;
	}

  return VTrue;
}

////////////////////////////

VBool
VSocket::Bind(const VCard port, const VBool localOnly,
			  const VBool checkIfInUse)
{
  struct sockaddr_in addr;

  // Check that the socket is open!
  if (sock < 0)
    return VFalse;

  // If a specific port is being set then check it's not already used!
  if (port != 0 && checkIfInUse)
  {
	VSocket dummy;

	if (dummy.Create())
	{
		// If we're able to connect then the port number is in use...
		if (dummy.Connect("localhost", port))
			return VFalse;
	}
  }

  // Set up the address to bind the socket to
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (localOnly)
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  else
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

  // And do the binding
  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
      return VFalse;

  return VTrue;
}

////////////////////////////

VBool
VSocket::Connect(VStringConst address, const VCard port)
{
  // Check the socket
  if (sock < 0)
    return VFalse;

  // Create an address structure and clear it
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));

  // Fill in the address if possible
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(address);

  // Was the string a valid IP address?
  if (addr.sin_addr.s_addr == -1)
    {
      // No, so get the actual IP address of the host name specified
      struct hostent *pHost;
      pHost = gethostbyname(address);
      if (pHost != NULL)
	  {
		  if (pHost->h_addr == NULL)
			  return VFalse;
		  addr.sin_addr.s_addr = ((struct in_addr *)pHost->h_addr)->s_addr;
	  }
	  else
	    return VFalse;
    }

  // Set the port number in the correct format
  addr.sin_port = htons(port);

  // Actually connect the socket
  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    return VFalse;

  // Put the socket into non-blocking mode
#ifdef __WIN32__
  u_long arg = 1;
  if (ioctlsocket(sock, FIONBIO, &arg) != 0)
	return VFalse;
#else
  if (fcntl(sock, F_SETFL, O_NDELAY) != 0)
	return VFalse;
#endif

  return VTrue;
}

////////////////////////////

VBool
VSocket::Listen()
{
  // Check socket
  if (sock < 0)
    return VFalse;

	// Set it to listen
  if (listen(sock, 5) < 0)
    return VFalse;

  return VTrue;
}

////////////////////////////

VSocket *
VSocket::Accept()
{
  const int one = 1;

  int new_socket_id;
  VSocket * new_socket;

  // Check this socket
  if (sock < 0)
    return NULL;

  // Accept an incoming connection
  if ((new_socket_id = accept(sock, NULL, 0)) < 0)
    return NULL;

  // Create a new VSocket and return it
  new_socket = new VSocket;
  if (new_socket != NULL)
    {
      new_socket->sock = new_socket_id;
    }
  else
    {
	  shutdown(new_socket_id, SD_BOTH);
	  closesocket(new_socket_id);
	  return NULL;
    }

  // Attempt to set the new socket's options
  setsockopt(new_socket->sock, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof(one));

  // Put the socket into non-blocking mode
#ifdef __WIN32__
  u_long arg = 1;
  if (ioctlsocket(new_socket->sock, FIONBIO, &arg) != 0) {
	delete new_socket;
	new_socket = NULL;
  }
#else
  if (fcntl(new_socket->sock, F_SETFL, O_NDELAY) != 0) {
	delete new_socket;
	new_socket = NULL;
  }
#endif

  return new_socket;
}

////////////////////////////

VBool
VSocket::TryAccept(VSocket **new_socket, long ms)
{
	// Check this socket
	if (sock < 0)
		return NULL;

	struct fd_set fds;
	struct timeval tm;
	FD_ZERO(&fds);
	FD_SET((unsigned int)sock, &fds);
	tm.tv_sec = ms / 1000;
	tm.tv_usec = (ms % 1000) * 1000;
	int ready = select(sock + 1, &fds, NULL, NULL, &tm);
	if (ready == 0) {
		// Timeout
		*new_socket = NULL;
		return VTrue;
	} else if (ready != 1) {
		// Error
		return VFalse;
	}
	// Ready to accept new connection
	VSocket *s = Accept();
	if (s == NULL)
		return VFalse;
	// Success
	*new_socket = s;
	return VTrue;
}

////////////////////////////

VString
VSocket::GetPeerName()
{
	struct sockaddr_in	sockinfo;
	struct in_addr		address;
	int					sockinfosize = sizeof(sockinfo);
	VString				name;

	// Get the peer address for the client socket
	getpeername(sock, (struct sockaddr *)&sockinfo, &sockinfosize);
	memcpy(&address, &sockinfo.sin_addr, sizeof(address));

	name = inet_ntoa(address);
	if (name == NULL)
		return "<unavailable>";
	else
		return name;
}

////////////////////////////

VString
VSocket::GetSockName()
{
	struct sockaddr_in	sockinfo;
	struct in_addr		address;
	int					sockinfosize = sizeof(sockinfo);
	VString				name;

	// Get the peer address for the client socket
	getsockname(sock, (struct sockaddr *)&sockinfo, &sockinfosize);
	memcpy(&address, &sockinfo.sin_addr, sizeof(address));

	name = inet_ntoa(address);
	if (name == NULL)
		return "<unavailable>";
	else
		return name;
}

////////////////////////////

VCard32
VSocket::Resolve(VStringConst address)
{
  VCard32 addr;

  // Try converting the address as IP
  addr = inet_addr(address);

  // Was it a valid IP address?
  if (addr == 0xffffffff)
    {
      // No, so get the actual IP address of the host name specified
      struct hostent *pHost;
      pHost = gethostbyname(address);
      if (pHost != NULL)
	  {
		  if (pHost->h_addr == NULL)
			  return 0;
		  addr = ((struct in_addr *)pHost->h_addr)->s_addr;
	  }
	  else
		  return 0;
    }

  // Return the resolved IP address as an integer
  return addr;
}

////////////////////////////

VBool
VSocket::SetTimeout(VCard32 secs)
{
	// WIN32S: the original refused outright on WinSock < 2.
	//
	// roytam1's winvnc333r9-vc4 patch (27786ef) simply commented the check out.
	// That is the right instinct but it leaves setsockopt's failure unhandled:
	// SO_RCVTIMEO is optional in WinSock 1.1 and most Windows 3.1 stacks reject
	// it.  Attempt it and tolerate refusal - a socket without a receive timeout
	// still works, it just blocks indefinitely on a dead peer, and the
	// single-threaded server polls with select() before reading anyway.
	int timeout=secs;
	VBool ok = VTrue;

	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR)
	{
		vnclog.Print(LL_SOCKINFO,
			VNCLOG("SO_RCVTIMEO not supported by this stack (WinSock error %d)\n"),
			WSAGetLastError());
		ok = VFalse;
	}
	if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR)
	{
		vnclog.Print(LL_SOCKINFO,
			VNCLOG("SO_SNDTIMEO not supported by this stack (WinSock error %d)\n"),
			WSAGetLastError());
		ok = VFalse;
	}

	// Report the truth to the caller, but note that both callers treat a
	// failure as non-fatal - grep SetTimeout in vncClient.cpp / vncServer.cpp.
	return ok;
}

////////////////////////////

VInt VSocket::Send(const char *buff, const VCard bufflen)
{
	errno = 0;

	VInt bytes = send(sock, buff, bufflen, 0);
	if (bytes < 0)
	{
		int wsa_error = WSAGetLastError();
		vnclog.Print(
			LL_SOCKERR,
			VNCLOG("send(0x%x, 0x%x, %d, 0): wsa_error = %d\n"),
			sock,
			buff,
			bufflen,
			wsa_error);
#ifdef __WIN32__
		if (wsa_error == WSAEWOULDBLOCK)
			errno = EWOULDBLOCK;
#endif
	}

	return bytes;
}

////////////////////////////

VBool
VSocket::SendExact(const char *buff, const VCard bufflen)
{
	struct fd_set write_fds;
	struct timeval tm;
	int count;

	// Put the data into the queue
	SendQueued(buff, bufflen);

	// WIN32S: same treatment as ReadExact - pump messages while waiting, and
	// bound the wait so a client that stops reading cannot hang the server.
	//
	// A stalled client is a realistic case here rather than a theoretical one:
	// the server generates update data faster than a 10 Mbit link and a slow
	// viewer can drain it, so out_queue can stay non-empty for a long time.
	const DWORD sendDeadlineMs = 30000;
	DWORD startTick = GetTickCount();

	while (out_queue) {
		// Wait until some data can be sent
		do {
			FD_ZERO(&write_fds);
			FD_SET((unsigned int)sock, &write_fds);
			tm.tv_sec = 0;
			tm.tv_usec = 50000;			// 50 ms, was a full second
			count = select(sock + 1, NULL, &write_fds, NULL, &tm);

			if (count == 0) {
				MSG msg;
				int pumped = 0;
				while (pumped++ < 8 && PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
					if (msg.message == WM_QUIT) {
						PostQuitMessage((int)msg.wParam);
						return VFalse;
					}
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}

				if ((DWORD)(GetTickCount() - startTick) > sendDeadlineMs) {
					vnclog.Print(LL_SOCKERR,
						VNCLOG("timed out sending; client is not reading\n"));
					return VFalse;
				}
			}
		} while (count == 0);
		if (count < 0 || count > 1) {
			vnclog.Print(LL_SOCKERR, VNCLOG("socket error in select(): %d\n"),
						 WSAGetLastError());
			return VFalse;
		}
		// Actually send some data
		if (FD_ISSET((unsigned int)sock, &write_fds)) {
			if (!SendFromQueue())
				return VFalse;
		}
    }

	return VTrue;
}

////////////////////////////

VBool
VSocket::SendQueued(const char *buff, const VCard bufflen)
{
	omni_mutex_lock l(queue_lock);

	// Just append new bytes to the output queue
	if (!out_queue) {
		out_queue = new AIOBlock(bufflen, buff);
		bytes_sent = 0;
	} else {
		AIOBlock *last = out_queue;
		while (last->next)
			last = last->next;
		last->next = new AIOBlock(bufflen, buff);
	}

	return VTrue;
}

////////////////////////////

VBool
VSocket::SendFromQueue()
{
	omni_mutex_lock l(queue_lock);

	// Is there something to send?
	if (!out_queue)
		return VTrue;

	// Maximum data size to send at once.
	//
	// WIN32S: was 32768 (0x8000).  That value is INT_MIN as a 16-bit int, and
	// the Win32s Winsock 1.1 thunk cannot marshal a 32K send down to the 16-bit
	// stack - send() fails with WSAEFAULT (10014) and the client is dropped.
	// Observed as Raw/RRE/CoRRE/full-screen-Hextile disconnecting on the first
	// update (first chunk is always full-size) while Tight/Zlib/ZlibHex (small
	// sends) and file transfers (8K blocks) worked.  16384 stays representable
	// and halves segment-boundary exposure.  Encoders are unaffected: they
	// produce identical bytes into the queue; only send() chunking changes.
	size_t portion_size = out_queue->data_size - bytes_sent;
	if (portion_size > 16384)
		portion_size = 16384;

	// Try to send some data
	int bytes = Send(out_queue->data_ptr + bytes_sent, portion_size);
	if (bytes > 0) {
		bytes_sent += bytes;
	} else if (bytes < 0 && errno != EWOULDBLOCK) {
		vnclog.Print(LL_SOCKERR, VNCLOG("socket error\n"));
		return VFalse;
	}

	// Remove block if all its data has been sent
	if (bytes_sent == out_queue->data_size) {
		AIOBlock *sent = out_queue;
		out_queue = sent->next;
		bytes_sent = 0;
		delete sent;
	}

	return VTrue;
}

////////////////////////////
//
// WIN32S SINGLE-THREADED ADDITIONS
//
// See the note in VSocket.h.  The server has no per-client thread any more, so
// the idle loop needs to ask "is there anything to do?" without blocking.
//

VBool
VSocket::HasData()
{
	if (sock < 0)
		return VFalse;

	struct fd_set read_fds;
	struct timeval tm;

	FD_ZERO(&read_fds);
	FD_SET((unsigned int)sock, &read_fds);
	tm.tv_sec = 0;
	tm.tv_usec = 0;

	// The first argument is ignored by WinSock (an fd_set is an array of
	// handles, not a bitmask), so sock+1 is not required - but pass it anyway
	// for consistency with the rest of this file.
	int count = select(sock + 1, &read_fds, NULL, NULL, &tm);
	if (count <= 0)
		return VFalse;

	return FD_ISSET((unsigned int)sock, &read_fds) ? VTrue : VFalse;
}

VBool
VSocket::FlushQueued()
{
	if (sock < 0)
		return VFalse;
	if (out_queue == NULL)
		return VTrue;

	struct fd_set write_fds;
	struct timeval tm;

	// Push out as much as the stack will accept right now.  Bounded so that a
	// large queued update cannot monopolise the single thread: the idle loop
	// will call us again on its next pass.
	int rounds = 0;
	while (out_queue != NULL && rounds++ < 16) {
		FD_ZERO(&write_fds);
		FD_SET((unsigned int)sock, &write_fds);
		tm.tv_sec = 0;
		tm.tv_usec = 0;

		int count = select(sock + 1, NULL, &write_fds, NULL, &tm);
		if (count < 0) {
			vnclog.Print(LL_SOCKERR,
				VNCLOG("socket error in FlushQueued select(): %d\n"),
				WSAGetLastError());
			return VFalse;
		}
		if (count == 0)
			break;					// would block; try again next time round

		if (!FD_ISSET((unsigned int)sock, &write_fds))
			break;

		if (!SendFromQueue())
			return VFalse;
	}

	return VTrue;
}

////////////////////////////

VInt
VSocket::Read(char *buff, const VCard bufflen)
{
	errno = 0;

	VInt bytes = recv(sock, buff, bufflen, 0);

#ifdef __WIN32__
	if (bytes < 0 && WSAGetLastError() == WSAEWOULDBLOCK)
		errno = EWOULDBLOCK;
#endif

	return bytes;
}

////////////////////////////

VBool
VSocket::ReadExact(char *buff, const VCard bufflen)
{
	int bytes;
	VCard currlen = bufflen;
	struct fd_set read_fds, write_fds;
	struct timeval tm;
	int count;

	// ==================================================================
	// WIN32S: ReadExact(NULL, n) MUST WORK - it is the "discard n bytes"
	// idiom used all over vncClient.cpp's file-transfer handlers, e.g.
	//
	//     m_socket->ReadExact(NULL, msg.fdr.fNameSize);   // skip the name
	//
	// The original relied on Read() -> recv(sock, NULL, len, 0), i.e. it
	// passed a NULL buffer straight to WinSock.  On NT that returns
	// WSAEFAULT and the loop treats it as a socket error, which happens to
	// look like "connection failed" rather than a crash - but on a
	// WinSock 1.1 stack under Win32s, recv() with a NULL buffer can fault
	// inside the 16-bit stack and take the whole VM down.
	//
	// Handle it explicitly: read into a scratch buffer and throw the data
	// away.
	// ==================================================================
	if (buff == NULL) {
		char discard[256];
		VCard remaining = bufflen;
		while (remaining > 0) {
			VCard chunk = (remaining > sizeof(discard)) ?
						  (VCard)sizeof(discard) : remaining;
			if (!ReadExact(discard, chunk))
				return VFalse;
			remaining -= chunk;
		}
		return VTrue;
	}

	// WIN32S NOTE ON THE WAIT LOOP BELOW.
	//
	// This function still blocks until the requested bytes arrive.  That is
	// deliberate and safe, because vncClient::PumpIdle() only calls into the
	// protocol code after VSocket::HasData() has reported that a message has
	// started arriving - so we are always completing a message the client has
	// already begun sending.  Do NOT call ReadExact speculatively from the idle
	// loop.
	//
	// Two changes from the original:
	//
	//  * The inner "do { select(...) } while (count == 0)" spun with a 50
	//    MICROSECOND timeout (tv_usec = 50, not 50000).  On a cooperatively
	//    scheduled system with one thread, that is a tight busy-loop that
	//    starves every other Windows task while it waits.  The timeout is now
	//    50 ms and the loop pumps messages, so the machine stays usable.
	//
	//  * The wait is bounded.  A client that opens a connection, sends one byte
	//    of a message header and then goes silent used to hang the server
	//    forever; now it disconnects after the timeout.
	const DWORD readDeadlineMs = 30000;		// 30 s for a partial message
	DWORD startTick = GetTickCount();

	while (currlen > 0) {
		// Wait until some data can be read or sent
		do {
			FD_ZERO(&read_fds);
			FD_SET((unsigned int)sock, &read_fds);
			FD_ZERO(&write_fds);
			if (out_queue)
				FD_SET((unsigned int)sock, &write_fds);
			tm.tv_sec = 0;
			tm.tv_usec = 50000;			// 50 ms, was 50 us
			count = select(sock + 1, &read_fds, &write_fds, NULL, &tm);

			if (count == 0) {
				// Nothing yet.  Give the rest of the system a chance to run -
				// this is the single application thread.
				MSG msg;
				int pumped = 0;
				while (pumped++ < 8 && PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
					if (msg.message == WM_QUIT) {
						PostQuitMessage((int)msg.wParam);
						return VFalse;
					}
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}

				if ((DWORD)(GetTickCount() - startTick) > readDeadlineMs) {
					vnclog.Print(LL_SOCKERR,
						VNCLOG("timed out waiting for %u more bytes\n"),
						(unsigned int)currlen);
					return VFalse;
				}
			}
		} while (count == 0);
		if (count < 0 || count > 2) {
			vnclog.Print(LL_SOCKERR, VNCLOG("socket error in select(): %d\n"),
						 WSAGetLastError());
			return VFalse;
		}
		if (FD_ISSET((unsigned int)sock, &write_fds)) {
			// Try to send some data
			if (!SendFromQueue())
				return VFalse;
		}
		if (FD_ISSET((unsigned int)sock, &read_fds)) {
			// Try to read some data in
			bytes = Read(buff, currlen);
			if (bytes > 0) {
				// Adjust the buffer position and size
				buff += bytes;
				currlen -= bytes;
			} else if (bytes < 0 && errno != EWOULDBLOCK) {
				vnclog.Print(LL_SOCKERR, VNCLOG("socket error\n"));
				return VFalse;
			} else if (bytes == 0) {
				vnclog.Print(LL_SOCKERR, VNCLOG("zero bytes read\n"));
				return VFalse;
			}
		}
    }

	return VTrue;
}

