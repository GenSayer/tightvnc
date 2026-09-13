//  Copyright (C) 2001 Constantin Kaplinsky. All Rights Reserved.
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


// vncClient.h

// vncClient class handles the following functions:
// - Recieves requests from the connected client and
//   handles them
// - Handles incoming updates properly, using a vncBuffer
//   object to keep track of screen changes
// It uses a vncBuffer and is passed the vncDesktop and
// vncServer to communicate with.

class vncClient;
typedef SHORT vncClientId;

#if (!defined(_WINVNC_VNCCLIENT))
#define _WINVNC_VNCCLIENT

//#include <list>
#include "list.h"	// WIN32S: local minimal list<> (MSVC 4.1 STL cannot build these)

typedef list<vncClientId> vncClientList;

// Includes
#include "stdhdrs.h"
#include "VSocket.h"
#include <omnithread.h>

// Custom
#include "rectlist.h"
#include "vncDesktop.h"
#include "vncRegion.h"
#include "vncBuffer.h"
#include "vncKeymap.h"

// The vncClient class itself
//
// ==========================================================================
// WIN32S SINGLE-THREADED DESIGN
//
// Originally each client ran its whole RFB conversation on its own thread
// (class vncClientThread, vncClient.cpp).  Win32s has no threads, so a client
// is now a state machine driven from the application's idle loop:
//
//   Init()      - as before, but instead of spawning a thread it runs the
//                 blocking handshake inline (version exchange, authentication,
//                 ClientInit, pixel format, interaction caps) and then arms the
//                 state machine.  A client that fails the handshake is rejected
//                 exactly as before.
//
//   PumpIdle()  - called repeatedly from the idle loop.  If a message has
//                 started arriving it reads and processes exactly one RFB
//                 message, then returns.  Also flushes queued output.  Never
//                 blocks waiting for a message that has not begun.
//
//   IsDead()    - TRUE once the conversation has ended.  vncServer deletes the
//                 client from a safe place rather than from inside a window
//                 procedure or mid-iteration over the client list.
//
// The handshake is still allowed to block: it happens during AddClient, which
// is itself triggered by a socket notification, and it is bounded by
// VSocket's read/send deadlines.
// ==========================================================================

class vncClient
{
public:
	// Constructor/destructor
	vncClient();
	~vncClient();

	// The former thread class is gone; this friend declaration is retained
	// because vncServer still befriends the same name in places.
	friend class vncClientThread;

	// Init
	virtual BOOL Init(vncServer *server,
						VSocket *socket,
						BOOL reverse,
						BOOL shared,
						vncClientId newid);

	// Kill
	// The server uses this to close the client socket, which makes the state
	// machine notice at its next PumpIdle() and mark itself dead.
	virtual void Kill();

	// ---- single-threaded session driver (see vncClient.cpp) --------------

	// Service at most one pending client message, and flush queued output.
	// Returns TRUE if it did some work, so the caller can pump again before
	// going back to sleep.
	BOOL PumpIdle();

	// TRUE when the conversation has finished and this object may be deleted.
	BOOL IsDead() { return m_dead; }

	// Mark the conversation finished.  Used by Kill() and by the pump on any
	// protocol or socket failure.
	void SetDead() { m_dead = TRUE; m_protocol_ready = FALSE; }

	// Client manipulation functions for use by the server
	virtual void SetBuffer(vncBuffer *buffer);

	// Update handling functions
	virtual void TriggerUpdate();
	virtual void UpdateMouse();
	virtual void UpdateRect(RECT &rect);
	virtual void UpdateRegion(vncRegion &region);
	virtual void CopyRect(RECT &dest, POINT &source);
	virtual void UpdateClipText(LPSTR text);
	virtual void UpdatePalette();

	// Has the client sent an input event?
	virtual BOOL RemoteEventReceived() {
		BOOL result = m_remoteevent;
		m_remoteevent = FALSE;
		return result;
	}

	virtual void SetCursorPosChanged() {
		if (time(NULL) - m_pointer_event_time > 1) {
			m_cursor_pos_changed = TRUE;
		}
	}

	// Functions for setting & getting the client settings
	virtual void EnableKeyboard(BOOL enable) { m_keyboardenabled = enable; }
	virtual void EnablePointer(BOOL enable)  { m_pointerenabled = enable;  }
	virtual void BlockInput(BOOL block) { m_inputblocked = block; }
	virtual BOOL IsKeyboardEnabled() { return m_keyboardenabled; }
	virtual BOOL IsPointerEnabled()  { return m_pointerenabled;  }
	virtual BOOL IsInputEnabled()    { return m_keyboardenabled || m_pointerenabled; }
	virtual BOOL IsInputBlocked()    { return m_inputblocked; }

	virtual const char *GetClientName();
	virtual const char *GetServerName();
	virtual vncClientId GetClientId() {return m_id;};

	BOOL SetNewFBSize(BOOL sendnewfb);
	BOOL IncrRgnRequested(){return !m_incr_rgn.IsEmpty();};
	BOOL FullRgnRequested(){return !m_full_rgn.IsEmpty();};
	void UpdateLocalFormat();



	// ---- handshake, formerly vncClientThread members --------------------
	//
	// These were virtual members of vncClientThread.  They are now members of
	// vncClient itself and run inline from Init().  Their bodies are unchanged
	// apart from "m_client->" becoming "this->" (i.e. nothing) and "m_socket"
	// resolving to the client's own socket.
protected:
	BOOL InitVersion();
	BOOL InitAuthenticate();
	int  GetAuthenticationType();
	void SendConnFailedMessage(const char *reasonString);
	BOOL SendTextStringMessage(const char *str);
	BOOL NegotiateTunneling();
	BOOL NegotiateAuthentication(int authType);
	BOOL AuthenticateNone();
	BOOL AuthenticateVNC();
	BOOL ReadClientInit();
	BOOL SendInteractionCaps();
	// NOTE: there is no separate SendServerInit().  The ServerInit message is
	// built and sent inline inside RunHandshake(), exactly where the old run()
	// did it, so that the desktop-name and shared-rect handling stays in one
	// place.

	// Runs the whole blocking handshake.  Returns FALSE if the client must be
	// rejected.
	BOOL RunHandshake();

	// Handles exactly one RFB message that has already started arriving.
	// Returns FALSE if the conversation must end.
	BOOL HandleOneMessage();

	// Path conversion helper used by the file transfer handlers (was a
	// vncClientThread member).
	char *ConvertPath(char *path);

	// Update routines
protected:
	BOOL SendUpdate();
	BOOL SendRFBMsg(CARD8 type, BYTE *buffer, int buflen);
	BOOL SendRectangles(rectlist &rects);
	BOOL SendRectangle(RECT &rect);
	BOOL SendCopyRect(RECT &dest, POINT &source);
	BOOL SendCursorShapeUpdate();
	BOOL SendCursorPosUpdate();
	BOOL SendLastRect();
	BOOL SendPalette();



	// Internal stuffs
protected:
	// Per-client settings
	int				m_protocol_minor_version;
	BOOL			m_protocol_tightvnc;
	BOOL			m_keyboardenabled;
	BOOL			m_pointerenabled;
	BOOL			m_inputblocked;
	BOOL			m_copyrect_use;
	vncClientId		m_id;

	// The screen buffer
	vncBuffer		*m_buffer;

	// The server
	vncServer		*m_server;

	// The socket
	VSocket			*m_socket;
	char			*m_client_name;
	char			*m_server_name;

	// Session state.
	//
	// m_dead replaces "the thread returned": the conversation is over and the
	// object can be freed by vncServer::ReapDeadClients().
	BOOL			m_dead;

	// ---- WIN32S input synthesis state (see vncClient.cpp) ----------------
	//
	// Windows 3.1 gives us no way to drive the system's modal move/resize loop or
	// to have double-clicks generated for us, because both depend on PHYSICAL
	// input that injected messages never provide.  Both are therefore implemented
	// here.

	// Caption drag: the window being dragged, and the grab point relative to its
	// top-left corner.  NULL when no drag is in progress.
	HWND			m_dragWindow;
	int				m_dragOffsetX;
	int				m_dragOffsetY;

	// Interactive resize: the window being resized, which edge/corner is grabbed,
	// the original rectangle, and whether a resize is in progress.  Replaces the
	// system's modal resize loop, which cannot be driven by posted messages.
	HWND			m_resizeWindow;
	int				m_resizeHit;
	RECT			m_resizeRect;
	BOOL			m_resizeActive;

	// Scrollbar tracking: which window scrollbar is being dragged, its
	// orientation and range, and the geometry needed to map cursor pixels to
	// scroll positions.  Replaces the system's modal scrollbar loop, which
	// polls the physical mouse.  Window scrollbars (HTVSCROLL/HTHSCROLL, e.g.
	// a Progman group's scrollbars) arrive here; ScrollBar CONTROLS are
	// separate child windows that hit-test as HTCLIENT and already work via
	// the client-area branch.
	HWND			m_scrollWindow;
	BOOL			m_scrollVert;
	BOOL			m_scrollActive;
	int				m_scrollMin;
	int				m_scrollMax;
	int				m_scrollArrow;
	int				m_scrollTrackLen;
	int				m_scrollStrip0;
	int				m_scrollLast;

	// Double-click detection: when and where the last press landed, and on which
	// window.  Compared against GetDoubleClickTime() and SM_CXDOUBLECLK.
	DWORD			m_lastClickTime;
	int				m_lastClickX;
	int				m_lastClickY;
	HWND			m_lastClickWindow;

	// Pending iconic single-click Control menu.  A quick press-release on a
	// minimized icon must show the icon menu, but a double-click must restore
	// instead - and the two cannot be told apart until the double-click window
	// (GetDoubleClickTime) passes.  UP arms this; PumpIdle fires it once the
	// window expires; a second press in time cancels it via the double-click
	// path.  NULL when nothing is pending.
	HWND			m_menuPendingWindow;
	DWORD			m_menuPendingTime;

	// Copies of the Init() arguments that the old thread object held.
	BOOL			m_reverse;
	BOOL			m_shared;

	// Flag to indicate whether the client is ready for RFB messages
	BOOL			m_protocol_ready;

	// Flag to indicate that our framebuffer size has changed before
	// the client has told that it supports NewFBSize message
	BOOL			m_fb_size_changed;

	// User input information
	RECT			m_oldmousepos;
	BOOL			m_mousemoved;
	rfbPointerEventMsg	m_ptrevent;

	// Support for cursor shape updates (XCursor, RichCursor encodings)
	BOOL			m_cursor_update_pending;
	BOOL			m_cursor_update_sent;
	BOOL			m_cursor_pos_changed;
	time_t			m_pointer_event_time;
	HCURSOR			m_hcursor;
	POINT			m_cursor_pos;

	// Region structures used when preparing updates
	// - region of rects which may have changed since last update
	// - region for which incremental data is requested
	// - region for which full data is requested
	vncRegion		m_changed_rgn;
	vncRegion		m_incr_rgn;
	vncRegion		m_full_rgn;
	omni_mutex		m_regionLock;

	BOOL			m_copyrect_set;
	RECT			m_copyrect_rect;
	POINT			m_copyrect_src;

	BOOL			m_updatewanted;
	RECT			m_fullscreen;

	// When the local display is palettized, it sometimes changes...
	BOOL			m_palettechanged;

	// Information used in polling mode!
	BOOL			m_remoteevent;

	BOOL			m_use_NewFBSize;
	BOOL			m_use_PointerPos;

	omni_mutex		m_sendUpdateLock;

private:
	unsigned int FiletimeToTime70(FILETIME filetime);
	void SendFileDownloadData(unsigned short sizeFile, char *pFile);
	void SendFileDownloadData(unsigned int mTime);
	void SendFileUploadCancel(unsigned short reasonLen, char *reason);
	void SendFileDownloadFailed(unsigned short reasonLen, char *reason);
	void CloseUndoneFileTransfer();
	BOOL m_bUploadStarted;
	BOOL m_bDownloadStarted;
	HANDLE m_hFileToRead;
	HANDLE m_hFileToWrite;
	char m_UploadFilename[MAX_PATH];
	char m_DownloadFilename[MAX_PATH];
	void Time70ToFiletime(unsigned int mTime, FILETIME *pFiletime);
	unsigned int m_modTime;
	unsigned int beginUploadTime;
	unsigned int endUploadTime;
	DWORD m_rfbBlockSize;
public:
	void SendFileDownloadPortion();
};

#endif
