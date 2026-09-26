//  Copyright (C) 2003-2006 Constantin Kaplinsky. All Rights Reserved.
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


// Many thanks to Randy Brown <rgb@inven.com> for providing the 3-button
// emulation code.

// This is the main source for a ClientConnection object.
// It handles almost everything to do with a connection to a server.
// The decoding of specific rectangle encodings is done in separate files.

#include "stdhdrs.h"

#include "vncviewer.h"

#ifdef UNDER_CE
#include "omnithreadce.h"
#else
#include "omnithread.h"
#endif
// SD_BOTH comes from win32s_fix.h (force-included).  It used to be defined
// twice in this file - once as 0x02 in the UNDER_CE branch and once as 2 lower
// down - which is harmless only because the values agree.

#include "ClientConnection.h"
#include "SessionDialog.h"
#include "LoginAuthDialog.h"
#include "AboutBox.h"
#include "FileTransfer.h"
#include "commctrl.h"
#include "Exception.h"
#include "Win32sApi.h"
extern "C" {
#include "vncauth.h"
#include "d3des.h"
}

// ==========================================================================
// WIN32S NOTE ON THE BLOCK THAT USED TO BE HERE
//
// This file previously declared missing APIs itself:
//
//     HBRUSH __stdcall GetSysColorBrush(int nIndex);
//     int __stdcall SetScrollInfo(HWND, int, LPSCROLLINFO, BOOL);
//
// That satisfies the compiler but guarantees a load-time failure on Win32s:
// the linker records "GetSysColorBrush" and "SetScrollInfo" as imports from
// USER32, the Win32s USER32 does not export them, and the loader refuses to
// start the process.  Nothing in the program ever runs, which is why the crash
// produced no message and no log line.
//
// Both are now reached through Win32sApi.h (Win32sGetSysColorBrush /
// Win32sSetScrollInfo), which resolves them at run time and falls back to
// Windows 3.1 equivalents.
//
// SCROLLINFO/SIF_* and the NOTIFYICONDATA family also moved to Win32sApi.h so
// there is only one definition of each.
// ==========================================================================

#ifndef WM_NOTIFY
#define WM_NOTIFY            0x004E
#endif

// WM_MOUSEWHEEL is Win95+ (and only actually delivered by NT4/Win98 and later).
// It was #define'd in the middle of a switch statement in WndProc; moved here
// and guarded, because the MSVC 4.1 SDK headers may or may not define it
// depending on WINVER, and redefinition is a warning-turned-noise.
// Nothing sends this message on Win32s, so the case is simply never taken.
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL        0x020A
#endif

// SND_APPLICATION was needed only by the PlaySound call in ReadBell, which has
// been replaced by Win32sPlayBell().  Kept (guarded) so that re-enabling the
// original code does not break the build.
#ifndef SND_APPLICATION
#define SND_APPLICATION      0x0080
#endif

#ifndef TBSTYLE_FLAT
#define TBSTYLE_FLAT 0x800
#endif

#ifndef TB_SETINDENT
#define TB_SETINDENT (WM_USER + 71)
#endif

#define INITIALNETBUFSIZE 4096
#define MAX_ENCODINGS 20
#define VWR_WND_CLASS_NAME _T("VNCviewer")

/*
 * Macro to compare pixel formats.
 */

#define PF_EQ(x,y)							\
	((x.bitsPerPixel == y.bitsPerPixel) &&				\
	 (x.depth == y.depth) &&					\
	 ((x.bigEndian == y.bigEndian) || (x.bitsPerPixel == 8)) &&	\
	 (x.trueColour == y.trueColour) &&				\
	 (!x.trueColour || ((x.redMax == y.redMax) &&			\
			    (x.greenMax == y.greenMax) &&		\
			    (x.blueMax == y.blueMax) &&			\
			    (x.redShift == y.redShift) &&		\
			    (x.greenShift == y.greenShift) &&		\
			    (x.blueShift == y.blueShift))))

const rfbPixelFormat vnc8bitFormat = {8, 8, 0, 1, 7,7,3, 5,2,0,0,0};
const rfbPixelFormat vnc16bitFormat = {16, 16, 0, 1, 31,63,31, 11,5,0,0,0};


// *************************************************************************
//  WIN32S SINGLE-THREADED DESIGN
//
//  Originally a connection used two threads: the main one for windows and
//  input, and a per-connection worker for receiving/decoding/drawing.  Win32s
//  has no threads at all, so there is now exactly one thread:
//
//    * Run() does the blocking connect and handshake (as before).  While it
//      runs, the modeless "Connecting..." dialog is pumped from SetStatus().
//    * StartSession() replaces start_undetached(): it sends the first update
//      request and flips m_sessionStarted/m_running.
//    * PumpIdle() is called from the application idle loop and services at
//      most one server message per call, only when data is actually waiting
//      (SocketHasData()).  It never blocks, so the UI stays responsive.
//    * Deletion is deferred: the window procedure only sets m_dead, and
//      VNCviewerApp32::ReapDeadConnections() does the delete once we are back
//      in the main loop and no longer inside a window procedure.
// *************************************************************************

ClientConnection::ClientConnection(VNCviewerApp *pApp) 
{
	Init(pApp);
}

ClientConnection::ClientConnection(VNCviewerApp *pApp, SOCKET sock) 
{
	Init(pApp);
	m_sock = sock;
	m_serverInitiated = true;
	struct sockaddr_in svraddr;
	int sasize = sizeof(svraddr);
	if (getpeername(sock, (struct sockaddr *) &svraddr, 
		&sasize) != SOCKET_ERROR) {
		_stprintf(m_host, _T("%d.%d.%d.%d"), 
			svraddr.sin_addr.S_un.S_un_b.s_b1, 
			svraddr.sin_addr.S_un.S_un_b.s_b2, 
			svraddr.sin_addr.S_un.S_un_b.s_b3, 
			svraddr.sin_addr.S_un.S_un_b.s_b4);
		m_port = svraddr.sin_port;
	} else {
		_tcscpy(m_host,_T("(unknown)"));
		m_port = 0;
	};
}

ClientConnection::ClientConnection(VNCviewerApp *pApp, LPTSTR host, int port)
{
	Init(pApp);
	_tcsncpy(m_host, host, MAX_HOST_NAME_LEN);
	m_port = port;
}

void ClientConnection::Init(VNCviewerApp *pApp)
{
	m_hwnd = NULL;
	m_hwnd1 = NULL;
	m_hwndscroll = NULL;
	m_hToolbar = NULL;
	m_desktopName = NULL;
	m_port = -1;
	m_serverInitiated = false;
	m_netbuf = NULL;
	m_netbufsize = 0;
	m_zlibbuf = NULL;
	m_zlibbufsize = 0;
	m_hwndNextViewer = NULL;	
	m_pApp = pApp;
	m_dormant = false;
	m_hBitmapDC = NULL;
	m_dibbuf = NULL;
	m_dibbufsize = 0;
	m_hBitmap = NULL;
	m_hPalette = NULL;
	m_passwdSet = false;

	m_connDlg = NULL;

	m_enableFileTransfers = false;
	m_fileTransferDialogShown = false;
	m_pFileTransfer = new FileTransfer(this, m_pApp);

	// We take the initial conn options from the application defaults
	m_opts = m_pApp->m_options;
	
	m_sock = INVALID_SOCKET;
	m_inReadExact = false;
	m_inWriteExact = false;
	m_pendingHead = NULL;
	m_pendingTail = NULL;
	m_bKillThread = false;
	m_dead = false;
	m_sessionStarted = false;
	m_threadStarted = true;
	m_running = false;
	m_pendingFormatChange = false;

	m_hScrollPos = 0; m_vScrollPos = 0;

	m_waitingOnEmulateTimer = false;
	m_emulatingMiddleButton = false;

	m_decompStreamInited = false;

	m_decompStreamRaw.total_in = ZLIBHEX_DECOMP_UNINITED;
	m_decompStreamEncoded.total_in = ZLIBHEX_DECOMP_UNINITED;

	for (int i = 0; i < 4; i++)
		m_tightZlibStreamActive[i] = false;

	prevCursorSet = false;
	rcCursorX = 0;
	rcCursorY = 0;
	// The rest of the soft-cursor state was never initialised here, yet
	// SoftCursorLockArea/SoftCursorMove read it on the very first mouse move
	// and pass it to BitBlt.  prevCursorSet==false guards most paths, but
	// rcCursorHidden is also tested directly.
	rcCursorHidden = false;
	rcLockSet = false;
	rcSource = NULL;
	rcMask = NULL;
	rcWidth = 0;
	rcHeight = 0;
	rcHotX = 0;
	rcHotY = 0;
	rcLockX = 0;
	rcLockY = 0;
	rcLockWidth = 0;
	rcLockHeight = 0;
	m_hSavedAreaBitmap = NULL;
	m_hSavedAreaDC = NULL;

	// Was uninitialised: SelectSecurityType/ReadServerInit and the toolbar code
	// both consult these.
	m_tightVncProtocol = false;
	m_initialClipboardSeen = false;
	m_cliwidth = 0;
	m_cliheight = 0;
	m_fullwinwidth = 0;
	m_fullwinheight = 0;
	m_winwidth = 0;
	m_winheight = 0;
	m_emulate3ButtonsTimer = 0;
	m_hSess = NULL;

	// Create a buffer for various network operations
	CheckBufferSize(INITIALNETBUFSIZE);

	m_pApp->RegisterConnection(this);
}

void ClientConnection::InitCapabilities()
{
	// Supported authentication methods
	m_authCaps.Add(rfbAuthNone, rfbStandardVendor, sig_rfbAuthNone,
				   "No authentication");
	m_authCaps.Add(rfbAuthVNC, rfbStandardVendor, sig_rfbAuthVNC,
				   "Standard VNC password authentication");

	// Known server->client message types
	m_serverMsgCaps.Add(rfbFileListData, rfbTightVncVendor,
						sig_rfbFileListData, "File list data");
	m_serverMsgCaps.Add(rfbFileDownloadData, rfbTightVncVendor,
						sig_rfbFileDownloadData, "File download data");
	m_serverMsgCaps.Add(rfbFileUploadCancel, rfbTightVncVendor,
						sig_rfbFileUploadCancel, "File upload cancel request");
	m_serverMsgCaps.Add(rfbFileDownloadFailed, rfbTightVncVendor,
						sig_rfbFileDownloadFailed, "File download failure notification");

	// Known client->server message types
	m_clientMsgCaps.Add(rfbFileListRequest, rfbTightVncVendor,
						sig_rfbFileListRequest, "File list request");
	m_clientMsgCaps.Add(rfbFileDownloadRequest, rfbTightVncVendor,
						sig_rfbFileDownloadRequest, "File download request");
	m_clientMsgCaps.Add(rfbFileUploadRequest, rfbTightVncVendor,
						sig_rfbFileUploadRequest, "File upload request");
	m_clientMsgCaps.Add(rfbFileUploadData, rfbTightVncVendor,
						sig_rfbFileUploadData, "File upload data");
	m_clientMsgCaps.Add(rfbFileDownloadCancel, rfbTightVncVendor,
						sig_rfbFileDownloadCancel, "File download cancel request");
	m_clientMsgCaps.Add(rfbFileUploadFailed, rfbTightVncVendor,
						sig_rfbFileUploadFailed, "File upload failure notification");

	// Supported encoding types
	m_encodingCaps.Add(rfbEncodingCopyRect, rfbStandardVendor,
					   sig_rfbEncodingCopyRect, "Standard CopyRect encoding");
	m_encodingCaps.Add(rfbEncodingRRE, rfbStandardVendor,
					   sig_rfbEncodingRRE, "Standard RRE encoding");
	m_encodingCaps.Add(rfbEncodingCoRRE, rfbStandardVendor,
					   sig_rfbEncodingCoRRE, "Standard CoRRE encoding");
	m_encodingCaps.Add(rfbEncodingHextile, rfbStandardVendor,
					   sig_rfbEncodingHextile, "Standard Hextile encoding");
	m_encodingCaps.Add(rfbEncodingZlib, rfbTridiaVncVendor,
					   sig_rfbEncodingZlib, "Zlib encoding from TridiaVNC");
	m_encodingCaps.Add(rfbEncodingZlibHex, rfbTridiaVncVendor,
					   sig_rfbEncodingZlibHex, "ZlibHex encoding from TridiaVNC");
	m_encodingCaps.Add(rfbEncodingTight, rfbTightVncVendor,
					   sig_rfbEncodingTight, "Tight encoding by Constantin Kaplinsky");

	// Supported "fake" encoding types
	m_encodingCaps.Add(rfbEncodingCompressLevel0, rfbTightVncVendor,
					   sig_rfbEncodingCompressLevel0, "Compression level");
	m_encodingCaps.Add(rfbEncodingQualityLevel0, rfbTightVncVendor,
					   sig_rfbEncodingQualityLevel0, "JPEG quality level");
	m_encodingCaps.Add(rfbEncodingXCursor, rfbTightVncVendor,
					   sig_rfbEncodingXCursor, "X-style cursor shape update");
	m_encodingCaps.Add(rfbEncodingRichCursor, rfbTightVncVendor,
					   sig_rfbEncodingRichCursor, "Rich-color cursor shape update");
	m_encodingCaps.Add(rfbEncodingPointerPos, rfbTightVncVendor,
					   sig_rfbEncodingPointerPos, "Pointer position update");
	m_encodingCaps.Add(rfbEncodingLastRect, rfbTightVncVendor,
					   sig_rfbEncodingLastRect, "LastRect protocol extension");
	m_encodingCaps.Add(rfbEncodingNewFBSize, rfbTightVncVendor,
					   sig_rfbEncodingNewFBSize, "Framebuffer size change");
}

// 
// Run() creates the connection if necessary, does the initial negotiations
// and then makes the session live.  From that point on the session is driven
// by PumpIdle() from the application's idle loop - there is no worker thread.
// If Run throws an Exception, the caller must delete the ClientConnection object.
//

void ClientConnection::Run()
{
	// Get the host name and port if we haven't got it

	if (m_port == -1) {
		GetConnectDetails();
	} else {
		if (m_pApp->m_options.m_listening) {
			m_opts.LoadOpt(m_opts.m_display, 
							KEY_VNCVIEWER_HISTORI);
		}
	}

	// Show the "Connecting..." dialog box.
	//
	// Now a modeless dialog on this thread (see ConnectingDialog.cpp).  It is
	// deleted in every exit path: on success below, and by ~ClientConnection if
	// any of the steps that follow throws - the caller deletes the connection
	// object on exception, which is what makes that safe.
	m_connDlg = new ConnectingDialog(m_pApp->m_instance, m_opts.m_display);

	// Connect if we're not already connected
	if (m_sock == INVALID_SOCKET)
		Connect();

	SetSocketOptions();

	NegotiateProtocolVersion();

	PerformAuthentication();

	// Set up windows etc 
	CreateDisplay();

	SendClientInit();
	ReadServerInit();

	// Only for protocol version 3.7t
	if (m_tightVncProtocol) {
		// Determine which protocol messages and encodings are supported.
		ReadInteractionCaps();
		// Enable file transfers only if the server supports that.
		m_enableFileTransfers = false;
		if ( m_clientMsgCaps.IsEnabled(rfbFileListRequest) &&
			 m_serverMsgCaps.IsEnabled(rfbFileListData) ) {
			m_enableFileTransfers = true;
		}
	}

	// Close the "Connecting..." dialog box if not closed yet.
	if (m_connDlg != NULL) {
		delete m_connDlg;
		m_connDlg = NULL;
	}

	EnableFullControlOptions();

	CreateLocalFramebuffer();
	
	SetupPixelFormat();
	
	SetFormatAndEncodings();

	// Make the session live.  Was: start_undetached() (a second thread).
	StartSession();
}

//
// StartSession - replaces the old worker-thread entry point.
//
// Everything that run_undetached() used to do before entering its receive
// loop happens here, on the one and only thread.  The receive loop itself now
// lives in PumpIdle().
//
void ClientConnection::StartSession()
{
	vnclog.Print(9, _T("Starting session (single-threaded)\n"));

	m_threadStarted = true;

	// Order matters here.  m_running must be true before the first update
	// request, because DoBlit() (reached from the WM_PAINT that the request's
	// reply triggers) returns immediately when !m_running - the original code
	// set m_running after the request, which was harmless only because the
	// reply could not arrive until the worker thread's loop began.  With one
	// thread there is no such ordering guarantee.
	m_running = true;
	m_sessionStarted = true;

	try {
		SendFullFramebufferUpdateRequest();

		RealiseFullScreenMode(false);

		if (m_hwnd1 != NULL)
			UpdateWindow(m_hwnd1);
	} catch (WarningException &e) {
		m_running = false;
		m_sessionStarted = false;
		e.Report();
		if (m_hwnd1 != NULL)
			PostMessage(m_hwnd1, WM_CLOSE, 0, 0);
		else
			m_dead = true;
	} catch (QuietException &e) {
		m_running = false;
		m_sessionStarted = false;
		e.Report();
		if (m_hwnd1 != NULL)
			PostMessage(m_hwnd1, WM_CLOSE, 0, 0);
		else
			m_dead = true;
	} catch (Exception &e) {
		m_running = false;
		m_sessionStarted = false;
		e.Report();
		if (m_hwnd1 != NULL)
			PostMessage(m_hwnd1, WM_CLOSE, 0, 0);
		else
			m_dead = true;
	}
}

static WNDCLASS wndclass;	// FIXME!

void ClientConnection::CreateDisplay() 
{
	// Create the window
	
	WNDCLASS wndclass;
	
	wndclass.style			= 0;
	wndclass.lpfnWndProc	= ClientConnection::Proc;
	wndclass.cbClsExtra		= 0;
	wndclass.cbWndExtra		= 0;
	wndclass.hInstance		= m_pApp->m_instance;
	wndclass.hIcon			= (HICON)LoadIcon(m_pApp->m_instance,
												MAKEINTRESOURCE(IDI_MAINICON));
	wndclass.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground	= (HBRUSH) Win32sGetSysColorBrush(COLOR_BTNFACE);
    wndclass.lpszMenuName	= (LPCTSTR)NULL;
	wndclass.lpszClassName	= VWR_WND_CLASS_NAME;

	RegisterClass(&wndclass);

	wndclass.style			= 0;
	wndclass.lpfnWndProc	= ClientConnection::ScrollProc;
	wndclass.cbClsExtra		= 0;
	wndclass.cbWndExtra		= 0;
	wndclass.hInstance		= m_pApp->m_instance;
	wndclass.hIcon			= NULL;
	wndclass.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground	= (HBRUSH) GetStockObject(BLACK_BRUSH);
    wndclass.lpszMenuName	= (LPCTSTR)NULL;
	wndclass.lpszClassName	= "ScrollClass";

	RegisterClass(&wndclass);
	 
	wndclass.style			= 0;
	wndclass.lpfnWndProc	= ClientConnection::Proc;
	wndclass.cbClsExtra		= 0;
	wndclass.cbWndExtra		= 0;
	wndclass.hInstance		= m_pApp->m_instance;
	wndclass.hIcon			= NULL;
	switch (m_pApp->m_options.m_localCursor) {
	case NOCURSOR:
		wndclass.hCursor	= LoadCursor(m_pApp->m_instance, 
										MAKEINTRESOURCE(IDC_NOCURSOR));
		break;
	case SMALLCURSOR:
		wndclass.hCursor	= LoadCursor(m_pApp->m_instance, 
										MAKEINTRESOURCE(IDC_SMALLDOT));
		break;
	case NORMALCURSOR:
		wndclass.hCursor	=LoadCursor(NULL,IDC_ARROW);
		break;
	case DOTCURSOR:
	default:
		wndclass.hCursor	= LoadCursor(m_pApp->m_instance, 
										MAKEINTRESOURCE(IDC_DOTCURSOR));
	}
	wndclass.hbrBackground	= (HBRUSH) GetStockObject(BLACK_BRUSH);
    wndclass.lpszMenuName	= (LPCTSTR)NULL;
	wndclass.lpszClassName	= "ChildClass";

	RegisterClass(&wndclass);
	
	m_hwnd1 = CreateWindow(VWR_WND_CLASS_NAME,
			      _T("VNCviewer"),
			      WS_BORDER|WS_CAPTION|WS_SYSMENU|WS_SIZEBOX|
				  WS_MINIMIZEBOX|WS_MAXIMIZEBOX|
				  WS_CLIPCHILDREN,
			      CW_USEDEFAULT,
			      CW_USEDEFAULT,
			      CW_USEDEFAULT,       // x-size
			      CW_USEDEFAULT,       // y-size
			      NULL,                // Parent handle
			      NULL,                // Menu handle
			      m_pApp->m_instance,
			      NULL);
	// The original code never checked this.  If CreateWindow fails, the
	// following SetWindowLong/GetSystemMenu calls operate on NULL and the
	// viewer dies mid-connection with no diagnostic - which matches the
	// "crashes when making a connection" report on every platform.
	if (m_hwnd1 == NULL) {
		vnclog.Print(0, _T("CreateWindow(main) failed: %d\n"), GetLastError());
		throw ErrorException("Failed to create the viewer window.");
	}
	SetWindowLong(m_hwnd1, GWL_USERDATA, (LONG) this);
	SetWindowLong(m_hwnd1, GWL_WNDPROC, (LONG)ClientConnection::WndProc1);
	ShowWindow(m_hwnd1, SW_HIDE);

	m_hwndscroll = CreateWindow("ScrollClass",
			      NULL,
			      WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_BORDER,
			      CW_USEDEFAULT,
			      CW_USEDEFAULT,
			      CW_USEDEFAULT,       // x-size
			      CW_USEDEFAULT,       // y-size
			      m_hwnd1,                // Parent handle
			      NULL,                // Menu handle
			      m_pApp->m_instance,
			      NULL);
	if (m_hwndscroll == NULL) {
		vnclog.Print(0, _T("CreateWindow(scroll) failed: %d\n"), GetLastError());
		throw ErrorException("Failed to create the viewer scroll window.");
	}
	SetWindowLong(m_hwndscroll, GWL_USERDATA, (LONG) this);
	ShowWindow(m_hwndscroll, SW_HIDE);
	
	// Create a memory DC which we'll use for drawing to
	// the local framebuffer
	m_hBitmapDC = CreateCompatibleDC(NULL);
	if (m_hBitmapDC == NULL) {
		// Win32s GDI has a small, fixed pool of DCs.  Every subsequent GDI call
		// in the paint path takes this handle, so failing here must abort the
		// connection rather than proceed with NULL.
		vnclog.Print(0, _T("CreateCompatibleDC failed\n"));
		throw ErrorException("Could not create a memory device context.");
	}

	// Set a suitable palette up
	if (GetDeviceCaps(m_hBitmapDC, RASTERCAPS) & RC_PALETTE) {
		vnclog.Print(3, _T("Palette-based display - %d entries, %d reserved\n"), 
			GetDeviceCaps(m_hBitmapDC, SIZEPALETTE), GetDeviceCaps(m_hBitmapDC, NUMRESERVED));
		BYTE buf[sizeof(LOGPALETTE)+216*sizeof(PALETTEENTRY)];
		LOGPALETTE *plp = (LOGPALETTE *) buf;
		int pepos = 0;
		for (int r = 5; r >= 0; r--) {
			for (int g = 5; g >= 0; g--) {
				for (int b = 5; b >= 0; b--) {
					plp->palPalEntry[pepos].peRed   = r * 255 / 5; 	
					plp->palPalEntry[pepos].peGreen = g * 255 / 5;
					plp->palPalEntry[pepos].peBlue  = b * 255 / 5;
					plp->palPalEntry[pepos].peFlags  = NULL;
					pepos++;
				}
			}
		}
		plp->palVersion = 0x300;
		plp->palNumEntries = 216;
		m_hPalette = CreatePalette(plp);
	}

	// Add stuff to System menu
	HMENU hsysmenu = GetSystemMenu(m_hwnd1, FALSE);
	if (!m_opts.m_restricted) {
		bool save_item_flags = (m_serverInitiated) ? MF_GRAYED : 0;
		AppendMenu(hsysmenu, MF_SEPARATOR, NULL, NULL);
		AppendMenu(hsysmenu, MF_STRING, IDC_OPTIONBUTTON,
				   _T("Connection &options...\tCtrl-Alt-Shift-O"));
		AppendMenu(hsysmenu, MF_STRING, ID_CONN_ABOUT,
				   _T("Connection &info\tCtrl-Alt-Shift-I"));
		AppendMenu(hsysmenu, MF_STRING, ID_REQUEST_REFRESH,
				   _T("Request screen &refresh\tCtrl-Alt-Shift-R"));
		AppendMenu(hsysmenu, MF_SEPARATOR, NULL, NULL);
		AppendMenu(hsysmenu, MF_STRING, ID_FULLSCREEN,
				   _T("&Full screen\tCtrl-Alt-Shift-F"));
		AppendMenu(hsysmenu, MF_STRING, ID_TOOLBAR,
				   _T("Show &toolbar\tCtrl-Alt-Shift-T"));
		AppendMenu(hsysmenu, MF_SEPARATOR, NULL, NULL);
		AppendMenu(hsysmenu, MF_STRING, ID_CONN_CTLALTDEL,
				   _T("Send Ctrl-Alt-&Del"));
		AppendMenu(hsysmenu, MF_STRING, ID_CONN_CTLESC,
				   _T("Send Ctrl-Esc"));
		AppendMenu(hsysmenu, MF_STRING, ID_CONN_CTLDOWN,
				   _T("Ctrl key down"));
		AppendMenu(hsysmenu, MF_STRING, ID_CONN_ALTDOWN,
				   _T("Alt key down"));
		AppendMenu(hsysmenu, MF_SEPARATOR, NULL, NULL);
		AppendMenu(hsysmenu, MF_STRING | MF_GRAYED, IDD_FILETRANSFER,
				   _T("Transf&er files...\tCtrl-Alt-Shift-E"));
		AppendMenu(hsysmenu, MF_SEPARATOR, NULL, NULL);
		AppendMenu(hsysmenu, MF_STRING, ID_NEWCONN,
				   _T("&New connection...\tCtrl-Alt-Shift-N"));
		AppendMenu(hsysmenu, save_item_flags, ID_CONN_SAVE_AS,
				   _T("&Save connection info as...\tCtrl-Alt-Shift-S"));
	}

	AppendMenu(hsysmenu, MF_SEPARATOR, NULL, NULL);
	AppendMenu(hsysmenu, MF_STRING, IDD_APP_ABOUT,
			   _T("&About TightVNC Viewer..."));
	if (m_opts.m_listening) {
		AppendMenu(hsysmenu, MF_SEPARATOR, NULL, NULL);
		AppendMenu(hsysmenu, MF_STRING, ID_CLOSEDAEMON,
				   _T("Close &listening daemon"));
	}
	DrawMenuBar(m_hwnd1);

	m_hToolbar = CreateToolbar();

	m_hwnd = CreateWindow("ChildClass",
			      NULL,
			      WS_CHILD | WS_CLIPSIBLINGS,
			      CW_USEDEFAULT,
			      CW_USEDEFAULT,
			      CW_USEDEFAULT,       // x-size
			      CW_USEDEFAULT,	   // y-size
			      m_hwndscroll,             // Parent handle
			      NULL,                // Menu handle
			      m_pApp->m_instance,
			      NULL);
	if (m_hwnd == NULL) {
		vnclog.Print(0, _T("CreateWindow(child) failed: %d\n"), GetLastError());
		throw ErrorException("Failed to create the viewer child window.");
	}
	m_opts.m_hWindow = m_hwnd;
	hotkeys.SetWindow(m_hwnd1);
    ShowWindow(m_hwnd, SW_HIDE);
		
	SetWindowLong(m_hwnd, GWL_USERDATA, (LONG) this);
	SetWindowLong(m_hwnd, GWL_WNDPROC, (LONG)ClientConnection::WndProc);
	
	if(pApp->m_options.m_toolbar) {
		CheckMenuItem(GetSystemMenu(m_hwnd1, FALSE),
					ID_TOOLBAR, MF_BYCOMMAND|MF_CHECKED);
	}
	SaveConnectionHistory();
	// record which client created this window
	
#ifndef _WIN32_WCE
	// We want to know when the clipboard changes, so
	// insert ourselves in the viewer chain. But doing
	// this will cause us to be notified immediately of
	// the current state.
	// We don't want to send that.
	//
	// This is now the ONLY place the viewer registers as a clipboard viewer
	// (GetConnectDetails used to do it too, with m_hwnd still NULL).  Note that
	// SetClipboardViewer legitimately returns NULL when we are the first viewer
	// in the chain, so a NULL m_hwndNextViewer is not an error - it is checked
	// wherever it is used (see ClientConnectionClipboard.cpp and WM_DESTROY).
	m_initialClipboardSeen = false;
	if (!m_opts.m_DisableClipboard)
		m_hwndNextViewer = SetClipboardViewer(m_hwnd);
#endif
}

HWND ClientConnection::CreateToolbar()
{
	const int MAX_TOOLBAR_BUTTONS = 20;
	TBBUTTON but[MAX_TOOLBAR_BUTTONS];
	memset(but, 0, sizeof(but));
	int i = 0;

	but[i].iBitmap		= 0;
	but[i].idCommand	= IDC_OPTIONBUTTON;
	but[i].fsState		= TBSTATE_ENABLED;
	but[i++].fsStyle	= TBSTYLE_BUTTON;

	but[i].iBitmap		= 1;
	but[i].idCommand	= ID_CONN_ABOUT;
	but[i].fsState		= TBSTATE_ENABLED;
	but[i++].fsStyle	= TBSTYLE_BUTTON;

	but[i++].fsStyle	= TBSTYLE_SEP;

	but[i].iBitmap		= 2;
	but[i].idCommand	= ID_FULLSCREEN;
	but[i].fsState		= TBSTATE_ENABLED;
	but[i++].fsStyle	= TBSTYLE_BUTTON;

	but[i].iBitmap		= 3;
	but[i].idCommand	= ID_REQUEST_REFRESH;
	but[i].fsState		= TBSTATE_ENABLED;
	but[i++].fsStyle	= TBSTYLE_BUTTON;

	but[i++].fsStyle	= TBSTYLE_SEP;

	but[i].iBitmap		= 4;
	but[i].idCommand	= ID_CONN_CTLALTDEL;
	but[i].fsState		= TBSTATE_ENABLED;
	but[i++].fsStyle	= TBSTYLE_BUTTON;

	but[i].iBitmap		= 5;
	but[i].idCommand	= ID_CONN_CTLESC;
	but[i].fsState		= TBSTATE_ENABLED;
	but[i++].fsStyle	= TBSTYLE_BUTTON;

	but[i].iBitmap		= 6;
	but[i].idCommand	= ID_CONN_CTLDOWN;
	but[i].fsState		= TBSTATE_ENABLED;
	but[i++].fsStyle	= TBSTYLE_CHECK;

	but[i].iBitmap		= 7;
	but[i].idCommand	= ID_CONN_ALTDOWN;
	but[i].fsState		= TBSTATE_ENABLED;
	but[i++].fsStyle	= TBSTYLE_CHECK;

	but[i++].fsStyle	= TBSTYLE_SEP;

	but[i].iBitmap		= 8;
	but[i].idCommand	= IDD_FILETRANSFER;
	but[i].fsState		= TBSTATE_INDETERMINATE;
	but[i++].fsStyle	= TBSTYLE_BUTTON;

	but[i++].fsStyle	= TBSTYLE_SEP;

	but[i].iBitmap		= 9;
	but[i].idCommand	= ID_NEWCONN;
	but[i].fsState		= TBSTATE_ENABLED;
	but[i++].fsStyle	= TBSTYLE_BUTTON;

	but[i].iBitmap		= 10;
	but[i].idCommand	= ID_CONN_SAVE_AS;
	but[i].fsState		= TBSTATE_ENABLED;
	but[i++].fsStyle	= TBSTYLE_BUTTON;

	but[i].iBitmap		= 11;
	but[i].idCommand	= ID_DISCONNECT;
	but[i].fsState		= TBSTATE_ENABLED;
	but[i++].fsStyle	= TBSTYLE_BUTTON;

	int numButtons = i;
	// Was assert(): a debug-only bounds check on a stack array.
	if (numButtons > MAX_TOOLBAR_BUTTONS)
		numButtons = MAX_TOOLBAR_BUTTONS;

	if (m_hwnd1 == NULL)
		return NULL;

	// Win32sCreateToolbarEx resolves CreateToolbarEx at run time and returns
	// NULL when COMCTL32 is unavailable - which is the normal case on Windows
	// 3.1.  All of the toolbar's commands are also on the window's system menu
	// and on the accelerator table, so a missing toolbar costs no function.
	//
	// TBSTYLE_FLAT is IE3+/COMCTL32 4.70 and is silently ignored by older
	// versions, so it is harmless to leave in.
	HWND hwndToolbar = Win32sCreateToolbarEx(m_hwnd1,
		WS_CHILD | TBSTYLE_TOOLTIPS | 
		WS_CLIPSIBLINGS | TBSTYLE_FLAT,
		ID_TOOLBAR, 12, m_pApp->m_instance,
		IDB_BITMAP1, (void *)but, numButtons, 0, 0, 0, 0, sizeof(TBBUTTON));

	if (hwndToolbar == NULL) {
		vnclog.Print(2, _T("No toolbar (common controls unavailable)\n"));
		return NULL;
	}

	SendMessage(hwndToolbar, TB_SETINDENT, 4, 0);

	return hwndToolbar;
}

void ClientConnection::SaveConnectionHistory()
{
	if (m_serverInitiated) {
		return;
	}

	// Read the connection history list from vncviewer.ini ([History],
	// values "0"..).

	// Determine maximum number of connections to remember.
	// Guard the low end too: m_historyLimit comes from the ini file, and a 0
	// or negative value made "new TCHAR[maxEntries*256]" a zero/negative-size
	// allocation followed by writes through connList[] - silent heap
	// corruption.  This runs on every connection, including the first.
	int maxEntries = pApp->m_options.m_historyLimit;
	if (maxEntries > 1024) {
		return;
	}
	if (maxEntries < 1) {
		maxEntries = 1;
	}

	// Allocate memory for the list of connections, 256 TCHARs an entry.
	const int entryBufferSize = 256;
	const int connListBufferSize = maxEntries * entryBufferSize;
	TCHAR *connListBuffer = new TCHAR[connListBufferSize];
	if (connListBuffer == NULL) {
		return;
	}
	memset(connListBuffer, 0, connListBufferSize * sizeof(TCHAR));

	// Index first characters of each entry for convenient access.
	TCHAR **connList = new TCHAR*[maxEntries];
	if (connList == NULL) {
		delete [] connListBuffer;
		return;
	}
	int i;
	for (i = 0; i < maxEntries; i++) {
		connList[i] = &connListBuffer[i * entryBufferSize];
	}

	// Read the list of connections and remove it from the ini file.
	int numRead = 0;
	for (i = 0; i < maxEntries; i++) {
		char valueName[16];
		sprintf(valueName, "%d", i);
		VNCOptions::IniGetString(VIEWER_INI_HISTORY, valueName,
								 connList[numRead], entryBufferSize);
		if (connList[numRead][0] != '\0') {
			numRead++;
		}
		VNCOptions::IniDeleteKey(VIEWER_INI_HISTORY, valueName);
	}

	// An empty display string would write "0"="" and poison the history:
	// every later launch would load an empty entry first.  There is nothing
	// useful to remember for a connection with no name.
	if (m_opts.m_display[0] == '\0') {
		delete [] connList;
		delete [] connListBuffer;
		return;
	}

	// Save current connection first.
	VNCOptions::IniSetString(VIEWER_INI_HISTORY, "0", m_opts.m_display);

	// Save the list of other connections.
	// Don't forget to exclude duplicates of current connection and make
	// sure the number of entries written will not exceed maxEntries.
	int numWritten = 1;
	for (i = 0; i < numRead && numWritten < maxEntries; i++) {
		if (_tcscmp(connList[i], m_opts.m_display) != 0) {
			char keyName[16];
			sprintf(keyName, "%d", numWritten);
			VNCOptions::IniSetString(VIEWER_INI_HISTORY, keyName, connList[i]);
			numWritten++;
		}
	}

	// Prune every entry that did not fit, not just the first one.  The old
	// "if (i < numRead)" deleted a single overflow section and left the rest
	// plus their stale [section] option blocks behind.
	for (; i < numRead; i++) {
		if (_tcscmp(connList[i], m_opts.m_display) != 0) {
			VNCOptions::IniDeleteSection(connList[i]);
		}
	}
	// Also drop any leftover numbered values beyond what we wrote (they are
	// from a time when the limit was higher).
	for (int k = numWritten; k < maxEntries; k++) {
		char keyName[16];
		sprintf(keyName, "%d", k);
		VNCOptions::IniDeleteKey(VIEWER_INI_HISTORY, keyName);
	}

	// The original code leaked both of these on every single connection.
	delete [] connList;
	delete [] connListBuffer;

	// Save connection options for current connection.
	m_opts.SaveOpt(m_opts.m_display, KEY_VNCVIEWER_HISTORI);
}

void ClientConnection::EnableFullControlOptions()
{
	if (m_opts.m_ViewOnly) {
		SwitchOffKey();
		EnableAction(IDD_FILETRANSFER, false);
		EnableAction(ID_CONN_CTLALTDEL, false);
		EnableAction(ID_CONN_CTLDOWN, false);
		EnableAction(ID_CONN_ALTDOWN, false);
		EnableAction(ID_CONN_CTLESC, false);
	} else {
		EnableAction(IDD_FILETRANSFER, m_enableFileTransfers);
		EnableAction(ID_CONN_CTLALTDEL, true);
		EnableAction(ID_CONN_CTLDOWN, true);
		EnableAction(ID_CONN_ALTDOWN, true);
		EnableAction(ID_CONN_CTLESC, true);
	}
}

void ClientConnection::EnableAction(int id, bool enable)
{
	if (m_hwnd1 == NULL)
		return;
	// SendMessage(NULL, ...) is not harmless: it is an invalid-window call.
	// m_hToolbar is NULL whenever the toolbar could not be created.
	if (enable) {
		EnableMenuItem(GetSystemMenu(m_hwnd1, FALSE), id,
					   MF_BYCOMMAND | MF_ENABLED);
		if (m_hToolbar != NULL)
			SendMessage(m_hToolbar, TB_SETSTATE, (WPARAM)id,
						(LPARAM)MAKELONG(TBSTATE_ENABLED, 0));
	} else {
		EnableMenuItem(GetSystemMenu(m_hwnd1, FALSE), id,
					   MF_BYCOMMAND | MF_GRAYED);
		if (m_hToolbar != NULL)
			SendMessage(m_hToolbar, TB_SETSTATE, (WPARAM)id,
						(LPARAM)MAKELONG(TBSTATE_INDETERMINATE, 0));
	}
}

void ClientConnection::SwitchOffKey()
{
	// WM_KILLFOCUS can arrive after the window has gone.
	if (m_hwnd1 == NULL)
		return;

	CheckMenuItem(GetSystemMenu(m_hwnd1, FALSE),
					ID_CONN_ALTDOWN, MF_BYCOMMAND|MF_UNCHECKED);
	CheckMenuItem(GetSystemMenu(m_hwnd1, FALSE),
					ID_CONN_CTLDOWN, MF_BYCOMMAND|MF_UNCHECKED);
	if (m_hToolbar != NULL) {
		SendMessage(m_hToolbar, TB_SETSTATE, (WPARAM)ID_CONN_CTLDOWN,
						(LPARAM)MAKELONG(TBSTATE_ENABLED, 0));
		SendMessage(m_hToolbar, TB_SETSTATE, (WPARAM)ID_CONN_ALTDOWN,
						(LPARAM)MAKELONG(TBSTATE_ENABLED, 0));
	}
	SendKeyEvent(XK_Alt_L,     false);
	SendKeyEvent(XK_Control_L, false);
	SendKeyEvent(XK_Shift_L,   false);
	SendKeyEvent(XK_Alt_R,     false);
	SendKeyEvent(XK_Control_R, false);
	SendKeyEvent(XK_Shift_R,   false);
}

void ClientConnection::GetConnectDetails()
{
	
	if (m_opts.m_configSpecified) {
		// LoadConnection returns -1 on failure.  The old code ignored the
		// result and carried straight on to Connect() with an empty m_host and
		// m_port == -1 - i.e. every bad/missing -config file turned into a
		// failed connect at best.
		if (LoadConnection(m_opts.m_configFilename, false) != 0)
			throw QuietException("Could not read the configuration file.");
	} else {
		SessionDialog sessdlg(&m_opts, this);
		if (!sessdlg.DoDialog()) {
			throw QuietException("User Cancelled");
		}
		// Add new connection to the connection history only if the VNC host name
		// was entered interactively, as we should remember user input even if it
		// does not seem to be correct. If the connection info was specified in
		// the command line or in a configuration file, it will be added after the
		// VNC connection is established successfully.
		SaveConnectionHistory();
	}
	// This is a bit of a hack: 
	// The config file may set various things in the app-level defaults which 
	// we don't want to be used except for the first connection. So we clear them
	// in the app defaults here.
	m_pApp->m_options.m_host[0] = '\0';
	m_pApp->m_options.m_port = -1;
	m_pApp->m_options.m_connectionSpecified = false;
	m_pApp->m_options.m_configSpecified = false;

	// NOTE: the original code called SetClipboardViewer(m_hwnd) here as well as
	// in CreateDisplay().  At this point in the sequence m_hwnd is still NULL
	// (CreateDisplay has not run yet), so this registered the *desktop* window
	// as a clipboard viewer and stored a bogus m_hwndNextViewer, which is later
	// used in ChangeClipboardChain and SendMessage.  Registration is done once,
	// correctly, at the end of CreateDisplay().
	m_initialClipboardSeen = false;
}

void ClientConnection::Connect()
{
	struct sockaddr_in thataddr;
	int res;

	if (m_connDlg != NULL)
		m_connDlg->SetStatus("Connection initiated");

	// Validate before touching the network.  m_host is filled from the command
	// line, a .vnc file or the New Connection dialog; an empty value reached
	// gethostbyname("") which behaves differently on every WinSock stack.
	if (m_host[0] == '\0')
		throw WarningException(_T("No VNC server host was specified."));
	if (m_port <= 0 || m_port > 65535) {
		char msg[128];
		_snprintf(msg, sizeof(msg) - 1, "Invalid port number (%d)", m_port);
		msg[sizeof(msg) - 1] = '\0';
		throw WarningException(msg);
	}

	// AF_INET, not PF_INET.  They have the same value, but some WinSock 1.1
	// stacks under Win32s validate the family constant strictly.
	m_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_sock == INVALID_SOCKET) {
		char msg[128];
		_snprintf(msg, sizeof(msg) - 1, "Error creating socket (WinSock error %d)",
				  WSAGetLastError());
		msg[sizeof(msg) - 1] = '\0';
		throw WarningException(msg);
	}
	
	memset(&thataddr, 0, sizeof(thataddr));

	// The host may be specified as a dotted address "a.b.c.d"
	// Try that first
	thataddr.sin_addr.s_addr = inet_addr(m_host);
	
	// If it wasn't one of those, do gethostbyname
	if (thataddr.sin_addr.s_addr == INADDR_NONE) {
		if (m_connDlg != NULL)
			m_connDlg->SetStatus("Looking up host name");

		LPHOSTENT lphost;
		lphost = gethostbyname(m_host);
		
		if (lphost == NULL) { 
			char msg[512];
			// _snprintf and a bounded %.255s: m_host is MAX_HOST_NAME_LEN and
			// the message also carries a fixed tail, so sprintf into 512 bytes
			// was not provably safe.
			_snprintf(msg, sizeof(msg) - 1,
					"Failed to get server address (%.255s).\n"
					"Did you type the host name correctly?", m_host);
			msg[sizeof(msg) - 1] = '\0';
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			throw WarningException(msg);
		};
		// Check the address family and length before copying: a stack that
		// returns an IPv6 or otherwise unexpected hostent would otherwise be
		// copied blind.
		if (lphost->h_addrtype != AF_INET || lphost->h_length < 4 ||
			lphost->h_addr_list == NULL || lphost->h_addr_list[0] == NULL) {
			closesocket(m_sock);
			m_sock = INVALID_SOCKET;
			throw WarningException("Server address is not an IPv4 address.");
		}
		memcpy(&thataddr.sin_addr, lphost->h_addr_list[0], 4);
	};
	
	thataddr.sin_family = AF_INET;
	thataddr.sin_port = htons((unsigned short)m_port);

	if (m_connDlg != NULL)
		m_connDlg->SetStatus("Connecting to server");

	// NOTE: this is a blocking connect, and under Win32s a blocking WinSock
	// call does not yield to other Windows tasks - the whole system appears
	// frozen until the TCP connect completes or times out.  That is accepted
	// here: making it asynchronous would mean restructuring the entire
	// handshake into a state machine.  The "Connecting..." dialog is shown and
	// updated around it so the user at least sees why.
	res = connect(m_sock, (LPSOCKADDR) &thataddr, sizeof(thataddr));
	if (res == SOCKET_ERROR) {
		char msg[512];
		_snprintf(msg, sizeof(msg) - 1,
				  "Failed to connect to server (%.255s)\r\nWinSock error %d",
				  m_opts.m_display, WSAGetLastError());
		msg[sizeof(msg) - 1] = '\0';
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
		throw WarningException(msg);
	}
	vnclog.Print(0, _T("Connected to %s port %d\n"), m_host, m_port);

	if (m_connDlg != NULL)
		m_connDlg->SetStatus("Connection established");
}

void ClientConnection::SetSocketOptions() {
	// Disable Nagle's algorithm.
	//
	// WIN32S: TCP_NODELAY is optional in WinSock 1.1 and several Windows 3.1
	// stacks (and the Microsoft TCP/IP-32 stack for Win32s) reject it.  The
	// original code threw a WarningException on failure, which aborted the
	// connection outright - a plausible cause of "crashes/fails on connect"
	// on those stacks.  Latency is worse without it, but the session works, so
	// log and continue.
	BOOL nodelayval = TRUE;
	if (setsockopt(m_sock, IPPROTO_TCP, TCP_NODELAY,
				   (const char *) &nodelayval, sizeof(nodelayval)) == SOCKET_ERROR) {
		vnclog.Print(1, _T("Could not disable Nagle's algorithm (WinSock error %d) - continuing\n"),
					 WSAGetLastError());
	}
}


void ClientConnection::NegotiateProtocolVersion()
{
	rfbProtocolVersionMsg pv;

	ReadExact(pv, sz_rfbProtocolVersionMsg);

    pv[sz_rfbProtocolVersionMsg] = 0;

	if (m_connDlg != NULL)
		m_connDlg->SetStatus("Server protocol version received");

	// XXX This is a hack.  Under CE we just return to the server the
	// version number it gives us without parsing it.  
	// Too much hassle replacing sscanf for now. Fix this!
#ifdef UNDER_CE
	m_minorVersion = 8;
#else
	int majorVersion, minorVersion;
	if (sscanf(pv, rfbProtocolVersionFormat, &majorVersion, &minorVersion) != 2) {
		// Log what we actually received: "Invalid protocol" with no detail is
		// the message a user sees when they point the viewer at, say, an HTTP
		// or SSH port, and it gives them nothing to work with.  pv is 12 bytes
		// of possibly non-printable data, so sanitise it first.
		char safe[sz_rfbProtocolVersionMsg + 1];
		int si;
		for (si = 0; si < sz_rfbProtocolVersionMsg; si++) {
			unsigned char c = (unsigned char)pv[si];
			safe[si] = (c >= 0x20 && c < 0x7f) ? (char)c : '.';
		}
		safe[sz_rfbProtocolVersionMsg] = '\0';
		vnclog.Print(0, _T("Unrecognised server greeting: \"%s\"\n"), safe);
		throw WarningException(_T("This does not look like a VNC server.\r\n"
								  "(Unrecognised protocol version greeting.)"));
	}
	vnclog.Print(0, _T("RFB server supports protocol version 3.%d\n"),
				 minorVersion);

	if (majorVersion == 3 && minorVersion >= 8) {
		m_minorVersion = 8;
	} else if (majorVersion == 3 && minorVersion == 7) {
		m_minorVersion = 7;
	} else {
		m_minorVersion = 3;
	}

	m_tightVncProtocol = false;

	// pv is rfbProtocolVersionMsg = char[13] and the formatted output is
	// exactly 12 characters plus a NUL, so this fits precisely.  Left as
	// sprintf because the format and both arguments are fixed here.
    sprintf(pv, rfbProtocolVersionFormat, 3, m_minorVersion);
#endif

    WriteExact(pv, sz_rfbProtocolVersionMsg);

	if (m_connDlg != NULL)
		m_connDlg->SetStatus("Protocol version negotiated");

	vnclog.Print(0, _T("Connected to RFB server, using protocol version 3.%d\n"),
				 m_minorVersion);
}

//
// Negotiate authentication scheme and authenticate if necessary
//

void ClientConnection::PerformAuthentication()
{
	int secType;
	if (m_minorVersion >= 7) {
		secType = SelectSecurityType();
	} else {
		secType = ReadSecurityType();
	}

	switch (secType) {
    case rfbSecTypeNone:
		Authenticate(rfbAuthNone);
		m_authScheme = rfbAuthNone;
		break;
    case rfbSecTypeVncAuth:
		Authenticate(rfbAuthVNC);
		m_authScheme = rfbAuthVNC;
		break;
    case rfbSecTypeTight:
		m_tightVncProtocol = true;
		InitCapabilities();
		SetupTunneling();
		PerformAuthenticationTight();
		break;
	default:	// should never happen
		vnclog.Print(0, _T("Internal error: Invalid security type\n"));
		throw ErrorException("Internal error: Invalid security type");
    }
}

//
// Read security type from the server (protocol 3.3)
//

int ClientConnection::ReadSecurityType()
{
	// Read the authentication scheme.
	CARD32 secType;
	ReadExact((char *)&secType, sizeof(secType));
	secType = Swap32IfLE(secType);

    if (secType == rfbSecTypeInvalid)
		throw WarningException(ReadFailureReason());

	if (secType != rfbSecTypeNone && secType != rfbSecTypeVncAuth) {
		vnclog.Print(0, _T("Unknown security type from RFB server: %d\n"),
					 (int)secType);
		throw ErrorException("Unknown security type requested!");
    }

	if (m_connDlg != NULL)
		m_connDlg->SetStatus("Security type received");

	return (int)secType;
}

//
// Select security type from the server's list (protocol 3.7 and above)
//

int ClientConnection::SelectSecurityType()
{
	// Read the list of secutiry types.
	CARD8 nSecTypes;
	ReadExact((char *)&nSecTypes, sizeof(nSecTypes));
	if (nSecTypes == 0)
		throw WarningException(ReadFailureReason());

	char *secTypeNames[] = {"None", "VncAuth"};
	CARD8 knownSecTypes[] = {rfbSecTypeNone, rfbSecTypeVncAuth};
	int nKnownSecTypes = sizeof(knownSecTypes);

	// Read the list into a fixed on-stack buffer instead of new[].  nSecTypes
	// is a single byte, so 255 always suffices - and this removes two leaks:
	// the original returned early on rfbSecTypeTight and threw on failure,
	// never freeing secTypes in either path.
	CARD8 secTypes[256];
	ReadExact((char *)secTypes, nSecTypes);
	CARD8 secType = rfbSecTypeInvalid;

	if (m_connDlg != NULL)
		m_connDlg->SetStatus("List of security types received");

	// Find out if the server supports TightVNC protocol extensions
	int j;
	for (j = 0; j < (int)nSecTypes; j++) {
		if (secTypes[j] == rfbSecTypeTight) {
			secType = rfbSecTypeTight;
			WriteExact((char *)&secType, sizeof(secType));
			if (m_connDlg != NULL)
				m_connDlg->SetStatus("TightVNC protocol extensions enabled");
			vnclog.Print(8, _T("Enabling TightVNC protocol extensions\n"));
			return rfbSecTypeTight;
		}
	}

	// Find first supported security type
	for (j = 0; j < (int)nSecTypes; j++) {
		for (int i = 0; i < nKnownSecTypes; i++) {
			if (secTypes[j] == knownSecTypes[i]) {
				secType = secTypes[j];
				WriteExact((char *)&secType, sizeof(secType));
				if (m_connDlg != NULL)
					m_connDlg->SetStatus("Security type requested");
				vnclog.Print(8, _T("Choosing security type %s(%d)\n"),
							 secTypeNames[i], (int)secType);
				break;
			}
		}
		if (secType != rfbSecTypeInvalid) break;
    }

    if (secType == rfbSecTypeInvalid) {
		vnclog.Print(0, _T("Server did not offer supported security type\n"));
		throw ErrorException("Server did not offer supported security type!");
	}

	return (int)secType;
}

//
// Setup tunneling (protocol 3.7t, 3.8t)
//

void ClientConnection::SetupTunneling()
{
	rfbTunnelingCapsMsg caps;
	ReadExact((char *)&caps, sz_rfbTunnelingCapsMsg);
	caps.nTunnelTypes = Swap32IfLE(caps.nTunnelTypes);

	if (caps.nTunnelTypes) {
		ReadCapabilityList(&m_tunnelCaps, caps.nTunnelTypes);
		if (m_connDlg != NULL)
			m_connDlg->SetStatus("List of tunneling capabilities received");

		// We cannot do tunneling yet.
		CARD32 tunnelType = Swap32IfLE(rfbNoTunneling);
		WriteExact((char *)&tunnelType, sizeof(tunnelType));
		if (m_connDlg != NULL)
			m_connDlg->SetStatus("Tunneling type requested");
	}
}

//
// Negotiate authentication scheme (protocol 3.7t, 3.8t)
//

void ClientConnection::PerformAuthenticationTight()
{
	rfbAuthenticationCapsMsg caps;
	ReadExact((char *)&caps, sz_rfbAuthenticationCapsMsg);
	caps.nAuthTypes = Swap32IfLE(caps.nAuthTypes);

	if (m_connDlg != NULL)
		m_connDlg->SetStatus("Header of authentication capability list received");

	if (!caps.nAuthTypes) {
		vnclog.Print(0, _T("No authentication needed\n"));
		if (m_connDlg != NULL)
			m_connDlg->SetStatus("No authentication needed");
		Authenticate(rfbAuthNone);
		m_authScheme = rfbAuthNone;
	} else {
		ReadCapabilityList(&m_authCaps, caps.nAuthTypes);
		if (m_connDlg != NULL)
			m_connDlg->SetStatus("Authentication capability list received");
		if (!m_authCaps.NumEnabled()) {
			vnclog.Print(0, _T("No suitable authentication schemes offered by the server\n"));
			throw ErrorException("No suitable authentication schemes offered by the server");
		}

		// Use server's preferred authentication scheme.
		CARD32 authScheme = m_authCaps.GetByOrder(0);
		authScheme = Swap32IfLE(authScheme);
		WriteExact((char *)&authScheme, sizeof(authScheme));
		authScheme = Swap32IfLE(authScheme);	// convert it back
		if (m_connDlg != NULL)
			m_connDlg->SetStatus("Authentication scheme requested");
		Authenticate(authScheme);
		m_authScheme = authScheme;
	}
}

// The definition of a function implementing some authentication scheme.
// For an example, see ClientConnection::AuthenticateVNC, below.

typedef bool (ClientConnection::*AuthFunc)(char *, int);

// A wrapper function for different authentication schemes.

void ClientConnection::Authenticate(CARD32 authScheme)
{
	AuthFunc authFuncPtr;

	// Uncomment this if the "Connecting..." dialog box should be
	// closed prior to authentication.
	/***
	if (m_connDlg != NULL) {
		delete m_connDlg;
		m_connDlg = NULL;
	}
	***/

	switch(authScheme) {
	case rfbAuthNone:
		authFuncPtr = &ClientConnection::AuthenticateNone;
		break;
	case rfbAuthVNC:
		authFuncPtr = &ClientConnection::AuthenticateVNC;
		break;
	default:
		vnclog.Print(0, _T("Unknown authentication scheme: %d\n"),
					 (int)authScheme);
		throw ErrorException("Unknown authentication scheme!");
	}

	// GetDescription returns NULL for a scheme that was never Add()ed - which
	// is the normal case on the 3.3 path, where InitCapabilities() has not run.
	// "%s" with NULL prints "(null)" on this CRT rather than crashing, but do
	// not rely on that.
	{
		char *desc = m_authCaps.GetDescription(authScheme);
		vnclog.Print(0, _T("Authentication scheme: %s\n"),
					 (desc != NULL) ? desc : _T("(unnamed)"));
	}

	const int errorMsgSize = 256;
	CheckBufferSize(errorMsgSize);
	char *errorMsg = m_netbuf;
	memset(errorMsg, 0, errorMsgSize);
	bool wasError = !(this->*authFuncPtr)(errorMsg, errorMsgSize);
	errorMsg[errorMsgSize - 1] = '\0';

	// Report authentication error.
	if (wasError) {
		vnclog.Print(0, _T("%s\n"), errorMsg);
		if (m_connDlg != NULL)
			m_connDlg->SetStatus("Error during authentication");
		throw AuthException(errorMsg);
	}

	CARD32 authResult;
	if (authScheme == rfbAuthNone && m_minorVersion < 8) {
		// In protocol versions prior to 3.8, "no authentication" is a
		// special case - no "security result" is sent by the server.
		authResult = rfbAuthOK;
	} else {
		ReadExact((char *) &authResult, 4);
		authResult = Swap32IfLE(authResult);
	}

	switch (authResult) {
	case rfbAuthOK:
		if (m_connDlg != NULL)
			m_connDlg->SetStatus("Authentication successful");
		vnclog.Print(0, _T("Authentication successful\n"));
		return;
	case rfbAuthFailed:
		if (m_minorVersion >= 8) {
			errorMsg = ReadFailureReason();
		} else {
			errorMsg = "Authentication failure";
		}
		break;
	case rfbAuthTooMany:
		errorMsg = "Authentication failure, too many tries";
		break;
	default:
		// m_netbuf is at least errorMsgSize (256) here because of the
		// CheckBufferSize above, but _snprintf does not guarantee termination
		// when it truncates.
		_snprintf(m_netbuf, 255, "Unknown authentication result (%d)",
				 (int)authResult);
		m_netbuf[255] = '\0';
		errorMsg = m_netbuf;
		break;
	}

	// Report authentication failure.
	vnclog.Print(0, _T("%s\n"), errorMsg);
	if (m_connDlg != NULL)
		m_connDlg->SetStatus(errorMsg);
	throw AuthException(errorMsg);
}

// "Null" authentication.

bool ClientConnection::AuthenticateNone(char *errBuf, int errBufSize)
{
	return true;
}

// The standard VNC authentication.
//
// An authentication function should return false on error and true if
// the authentication process was successful. Note that returning true
// does not mean that authentication was passed by the server, the
// server's result will be received and analyzed later.
// If false is returned, then a text error message should be copied
// to errorBuf[], no more than errBufSize bytes should be copied into
// that buffer.

bool ClientConnection::AuthenticateVNC(char *errBuf, int errBufSize)
{
    CARD8 challenge[CHALLENGESIZE];
	ReadExact((char *)challenge, CHALLENGESIZE);

	char passwd[MAXPWLEN + 1];
	memset(passwd, 0, sizeof(passwd));
	// Was the password already specified in a config file?
	if (m_passwdSet) {
		char *pw = vncDecryptPasswd(m_encPasswd);
		if (pw == NULL) {
			_snprintf(errBuf, errBufSize, "Could not decrypt stored password");
			return false;
		}
		// vncDecryptPasswd returns a MAXPWLEN buffer, but bound the copy
		// anyway: strcpy into a MAXPWLEN+1 array from a library buffer is
		// exactly the kind of thing that only bites on the small stacks Win32s
		// gives you.
		strncpy(passwd, pw, MAXPWLEN);
		passwd[MAXPWLEN] = '\0';
		free(pw);
	} else {
		LoginAuthDialog ad(m_opts.m_display, "Standard VNC Authentication");
		ad.DoDialog();
#ifndef UNDER_CE
		strncpy(passwd, ad.m_passwd, MAXPWLEN);
		passwd[MAXPWLEN]= '\0';
#else
		// FIXME: Move wide-character translations to a separate class
		int origlen = _tcslen(ad.m_passwd);
		int newlen = WideCharToMultiByte(
			CP_ACP,    // code page
			0,         // performance and mapping flags
			ad.m_passwd, // address of wide-character string
			origlen,   // number of characters in string
			passwd,    // address of buffer for new string
			255,       // size of buffer
			NULL, NULL);

		passwd[newlen]= '\0';
#endif
		if (strlen(passwd) == 0) {
			_snprintf(errBuf, errBufSize, "Empty password");
			errBuf[errBufSize - 1] = '\0';
			return false;
		}
		if (strlen(passwd) > 8) {
			passwd[8] = '\0';
		}
		vncEncryptPasswd(m_encPasswd, passwd);
		m_passwdSet = true;
	}				

	vncEncryptBytes(challenge, passwd);

	/* Lose the plain-text password from memory */
	memset(passwd, 0, sizeof(passwd));

	WriteExact((char *) challenge, CHALLENGESIZE);

	return true;
}

void ClientConnection::SendClientInit()
{
    rfbClientInitMsg ci;
	ci.shared = m_opts.m_Shared;

    WriteExact((char *)&ci, sz_rfbClientInitMsg);

	if (m_connDlg != NULL)
		m_connDlg->SetStatus("Client initialization message sent");
}

void ClientConnection::ReadServerInit()
{
    ReadExact((char *)&m_si, sz_rfbServerInitMsg);
	
	if (m_connDlg != NULL)
		m_connDlg->SetStatus("Server initialization message received");

    m_si.framebufferWidth = Swap16IfLE(m_si.framebufferWidth);
    m_si.framebufferHeight = Swap16IfLE(m_si.framebufferHeight);
    m_si.format.redMax = Swap16IfLE(m_si.format.redMax);
    m_si.format.greenMax = Swap16IfLE(m_si.format.greenMax);
    m_si.format.blueMax = Swap16IfLE(m_si.format.blueMax);
    m_si.nameLength = Swap32IfLE(m_si.nameLength);

	// Sanity-check what the server told us before we allocate from it or size
	// a bitmap with it.  A bogus nameLength here means a "new TCHAR[huge]"
	// followed by a blocking read of that many bytes; a bogus geometry means
	// CreateCompatibleBitmap for a framebuffer that cannot exist.  Neither was
	// checked before, and on Win32s either one is an immediate hard failure at
	// exactly the point the user reports the crash.
	if (m_si.nameLength > 1024) {
		vnclog.Print(0, _T("Bad desktop name length %u from server\n"),
					 (unsigned int)m_si.nameLength);
		throw ErrorException("Protocol error: implausible desktop name length.");
	}
	if (m_si.framebufferWidth == 0 || m_si.framebufferHeight == 0 ||
		m_si.framebufferWidth > 4096 || m_si.framebufferHeight > 4096) {
		vnclog.Print(0, _T("Bad framebuffer geometry %d x %d from server\n"),
					 (int)m_si.framebufferWidth, (int)m_si.framebufferHeight);
		throw ErrorException("Protocol error: implausible framebuffer size.");
	}
	
    m_desktopName = new TCHAR[m_si.nameLength + 2];
	if (m_desktopName == NULL)
		throw ErrorException("Out of memory reading desktop name.");

#ifdef UNDER_CE
    char *deskNameBuf = new char[m_si.nameLength + 2];

	ReadString(deskNameBuf, m_si.nameLength);
    
	MultiByteToWideChar( CP_ACP,   MB_PRECOMPOSED, 
			     deskNameBuf, m_si.nameLength,
			     m_desktopName, m_si.nameLength+1);
    delete deskNameBuf;
#else
    ReadString(m_desktopName, m_si.nameLength);
#endif

	// The desktop name is arbitrary server-supplied bytes; make sure it is a
	// terminated, printable-ish string before it goes into a window title and a
	// log line.  ReadString() terminates it, but embedded control characters
	// (including a stray CR/LF) upset the 3.1 title bar.
	m_desktopName[m_si.nameLength] = _T('\0');
	for (unsigned int ni = 0; ni < m_si.nameLength; ni++) {
		if ((unsigned char)m_desktopName[ni] < 0x20)
			m_desktopName[ni] = _T(' ');
	}

	// One SetWindowText, not two (the original called it twice with the same
	// argument, once before and once after the logging).
	SetWindowText(m_hwnd1, m_desktopName);

	vnclog.Print(0, _T("Desktop name \"%s\"\n"),m_desktopName);
	vnclog.Print(1, _T("Geometry %d x %d depth %d\n"),
		m_si.framebufferWidth, m_si.framebufferHeight, m_si.format.depth );

	SizeWindow(true);
}

//
// In protocols 3.7t/3.8t, the server informs us about supported
// protocol messages and encodings. Here we read this information.
//

void ClientConnection::ReadInteractionCaps()
{
	// Read the counts of list items following
	rfbInteractionCapsMsg intr_caps;
	ReadExact((char *)&intr_caps, sz_rfbInteractionCapsMsg);
	intr_caps.nServerMessageTypes = Swap16IfLE(intr_caps.nServerMessageTypes);
	intr_caps.nClientMessageTypes = Swap16IfLE(intr_caps.nClientMessageTypes);
	intr_caps.nEncodingTypes = Swap16IfLE(intr_caps.nEncodingTypes);
	if (m_connDlg != NULL)
		m_connDlg->SetStatus("Interaction capability list header received");

	// Read the lists of server- and client-initiated messages
	ReadCapabilityList(&m_serverMsgCaps, intr_caps.nServerMessageTypes);
	ReadCapabilityList(&m_clientMsgCaps, intr_caps.nClientMessageTypes);
	ReadCapabilityList(&m_encodingCaps, intr_caps.nEncodingTypes);
	if (m_connDlg != NULL)
		m_connDlg->SetStatus("Interaction capability list received");
}

//
// Read the list of rfbCapabilityInfo structures and enable corresponding
// capabilities in the specified container. The count argument specifies how
// many records to read from the socket.
//

void ClientConnection::ReadCapabilityList(CapsContainer *caps, int count)
{
	// count comes straight off the wire (a 16-bit field).  Bound it: each
	// iteration does a blocking ReadExact and a map insert, so a large value
	// from a broken or hostile server means a long stall and a lot of
	// allocation.  256 is far more than any real server sends.
	if (count < 0 || count > 256) {
		vnclog.Print(0, _T("Implausible capability count %d from server\n"), count);
		throw ErrorException("Protocol error: bad capability list length.");
	}

	rfbCapabilityInfo msginfo;
	for (int i = 0; i < count; i++) {
		ReadExact((char *)&msginfo, sz_rfbCapabilityInfo);
		msginfo.code = Swap32IfLE(msginfo.code);
		caps->Enable(&msginfo);
	}
}

void ClientConnection::SizeWindow(bool centered)
{
	if (m_hwnd1 == NULL || m_hwndscroll == NULL)
		return;

	// Find how large the desktop work area is.
	// Win32sGetWorkArea() always fills the rect (screen size on Win32s, real
	// work area elsewhere).  The old code passed an uninitialised RECT to
	// SystemParametersInfo and used it even if the call failed.
	RECT workrect;
	Win32sGetWorkArea(&workrect);
	int workwidth = workrect.right -  workrect.left;
	int workheight = workrect.bottom - workrect.top;
	vnclog.Print(2, _T("Screen work area is %d x %d\n"),
				 workwidth, workheight);

	RECT fullwinrect;
	
	if (m_opts.m_scaling) {
		SetRect(&fullwinrect, 0, 0,
				m_si.framebufferWidth * m_opts.m_scale_num / m_opts.m_scale_den,
				m_si.framebufferHeight * m_opts.m_scale_num / m_opts.m_scale_den);
	} else {
		SetRect(&fullwinrect, 0, 0,
				m_si.framebufferWidth, m_si.framebufferHeight);
	}	

	AdjustWindowRectEx(&fullwinrect, 
			   GetWindowLong(m_hwnd, GWL_STYLE ), 
			   FALSE, GetWindowLong(m_hwnd, GWL_EXSTYLE));

	m_fullwinwidth = fullwinrect.right - fullwinrect.left;
	m_fullwinheight = fullwinrect.bottom - fullwinrect.top;

	AdjustWindowRectEx(&fullwinrect, 
			   GetWindowLong(m_hwndscroll, GWL_STYLE ) & ~WS_HSCROLL & 
			   ~WS_VSCROLL & ~WS_BORDER, 
			   FALSE, GetWindowLong(m_hwndscroll, GWL_EXSTYLE));
	AdjustWindowRectEx(&fullwinrect, 
			   GetWindowLong(m_hwnd1, GWL_STYLE ), 
			   FALSE, GetWindowLong(m_hwnd1, GWL_EXSTYLE));

	// m_hToolbar is NULL when the common controls library is unavailable
	// (Win32s) - GetWindowRect would then leave rtb uninitialised and the
	// window height would be computed from stack junk.
	if (m_hToolbar != NULL &&
		GetMenuState(GetSystemMenu(m_hwnd1, FALSE),
					 ID_TOOLBAR, MF_BYCOMMAND) == MF_CHECKED) {
		RECT rtb;
		if (GetWindowRect(m_hToolbar, &rtb))
			fullwinrect.bottom = fullwinrect.bottom + rtb.bottom - rtb.top - 3;
	}

	m_winwidth  = min(fullwinrect.right - fullwinrect.left,  workwidth);
	m_winheight = min(fullwinrect.bottom - fullwinrect.top, workheight);
	if ((fullwinrect.right - fullwinrect.left > workwidth) &&
		(workheight - m_winheight >= 16)) {
		m_winheight = m_winheight + 16;
	} 
	if ((fullwinrect.bottom - fullwinrect.top > workheight) && 
		(workwidth - m_winwidth >= 16)) {
		m_winwidth = m_winwidth + 16;
	}

	int x,y;
	WINDOWPLACEMENT winplace;
	winplace.length = sizeof(WINDOWPLACEMENT);
	Win32sGetWindowPlacement(m_hwnd1, &winplace);
	if (centered) {
		x = (workwidth - m_winwidth) / 2;		
		y = (workheight - m_winheight) / 2;		
	} else {
		// Try to preserve current position if possible
		Win32sGetWindowPlacement(m_hwnd1, &winplace);
		if ((winplace.showCmd == SW_SHOWMAXIMIZED) || (winplace.showCmd == SW_SHOWMINIMIZED)) {
			x = winplace.rcNormalPosition.left;
			y = winplace.rcNormalPosition.top;
		} else {
			RECT tmprect;
			GetWindowRect(m_hwnd1, &tmprect);
			x = tmprect.left;
			y = tmprect.top;
		}
		if (x + m_winwidth > workrect.right)
			x = workrect.right - m_winwidth;
		if (y + m_winheight > workrect.bottom)
			y = workrect.bottom - m_winheight;
	}
	winplace.rcNormalPosition.top = y;
	winplace.rcNormalPosition.left = x;
	winplace.rcNormalPosition.right = x + m_winwidth;
	winplace.rcNormalPosition.bottom = y + m_winheight;
	Win32sSetWindowPlacement(m_hwnd1, &winplace);
	Win32sSetForegroundWindow(m_hwnd1);
	PositionChildWindow();
}

void ClientConnection::PositionChildWindow()
{	
	// Called from WM_SIZE, which can also arrive while the window is being
	// destroyed.
	if (m_hwnd1 == NULL || m_hwndscroll == NULL)
		return;

	RECT rparent;
	GetClientRect(m_hwnd1, &rparent);
	
	int parentwidth = rparent.right - rparent.left;
	int parentheight = rparent.bottom - rparent.top; 
				
	if (m_hToolbar != NULL &&
		GetMenuState(GetSystemMenu(m_hwnd1, FALSE),
				ID_TOOLBAR, MF_BYCOMMAND) == MF_CHECKED) {
		RECT rtb;
		if (GetWindowRect(m_hToolbar, &rtb)) {
			int rtbheight = rtb.bottom - rtb.top - 3;
			SetWindowPos(m_hToolbar, HWND_TOP, rparent.left, rparent.top,
						parentwidth, rtbheight, SWP_SHOWWINDOW);		
			parentheight = parentheight - rtbheight;
			rparent.top = rparent.top + rtbheight;
		}
	} else if (m_hToolbar != NULL) {
		ShowWindow(m_hToolbar, SW_HIDE);
	}
	
	SetWindowPos(m_hwndscroll, HWND_TOP, rparent.left - 1, rparent.top - 1,
					parentwidth + 2, parentheight + 2, SWP_SHOWWINDOW);
	if (!m_opts.m_FitWindow) {
		if (InFullScreenMode()) {				
			ShowScrollBar(m_hwndscroll, SB_HORZ, FALSE);
			ShowScrollBar(m_hwndscroll, SB_VERT, FALSE);
		} else {
			ShowScrollBar(m_hwndscroll, SB_VERT, parentheight < m_fullwinheight);
			ShowScrollBar(m_hwndscroll, SB_HORZ, parentwidth  < m_fullwinwidth);
			GetClientRect(m_hwndscroll, &rparent);	
			parentwidth = rparent.right - rparent.left;
			parentheight = rparent.bottom - rparent.top;
			ShowScrollBar(m_hwndscroll, SB_VERT, parentheight < m_fullwinheight);
			ShowScrollBar(m_hwndscroll, SB_HORZ, parentwidth  < m_fullwinwidth);
			GetClientRect(m_hwndscroll, &rparent);	
			parentwidth = rparent.right - rparent.left;
			parentheight = rparent.bottom - rparent.top;		
		}
	} else {
		if (!IsIconic(m_hwnd1)) {
			ShowScrollBar(m_hwndscroll, SB_HORZ, FALSE);
			ShowScrollBar(m_hwndscroll, SB_VERT, FALSE);
			GetClientRect(m_hwndscroll, &rparent);	
			parentwidth = rparent.right - rparent.left;
			parentheight = rparent.bottom - rparent.top;
			if ((parentwidth < 1) || (parentheight < 1))
				return;
			RECT fullwinrect;		
			int den = max(m_si.framebufferWidth * 100 / parentwidth,
							m_si.framebufferHeight * 100 / parentheight);
			SetRect(&fullwinrect, 0, 0, (m_si.framebufferWidth * 100 + den - 1) / den,
					(m_si.framebufferHeight * 100 + den - 1) / den);						
			while ((fullwinrect.right - fullwinrect.left > parentwidth) ||
					(fullwinrect.bottom - fullwinrect.top > parentheight)) {
				den++;
				SetRect(&fullwinrect, 0, 0, (m_si.framebufferWidth * 100 + den - 1) / den,
					(m_si.framebufferHeight * 100 + den - 1) / den);								
			}

			m_opts.m_scale_num = 100;
			m_opts.m_scale_den = den;
				
			m_opts.FixScaling();

			m_fullwinwidth = fullwinrect.right - fullwinrect.left;
			m_fullwinheight = fullwinrect.bottom - fullwinrect.top;
		}
	}

	
	int x, y;
	if (parentwidth  > m_fullwinwidth) {
		x = (parentwidth - m_fullwinwidth) / 2;
	} else {
		x = rparent.left;
	}
	if (parentheight > m_fullwinheight) {
		y = (parentheight - m_fullwinheight) / 2;
	} else {
		y = rparent.top;
	}
	
	SetWindowPos(m_hwnd, HWND_TOP, x, y,
					min(parentwidth, m_fullwinwidth),
					min(parentheight, m_fullwinheight),
					SWP_SHOWWINDOW);

	m_cliwidth = min( (int)parentwidth, (int)m_fullwinwidth);
	m_cliheight = min( (int)parentheight, (int)m_fullwinheight);

	m_hScrollMax = m_fullwinwidth;
	m_vScrollMax = m_fullwinheight;
           
		int newhpos, newvpos;
	if (!m_opts.m_FitWindow) {
		newhpos = max(0, min(m_hScrollPos, 
								 m_hScrollMax - max(m_cliwidth, 0)));
		newvpos = max(0, min(m_vScrollPos, 
				                 m_vScrollMax - max(m_cliheight, 0)));
	} else {
		newhpos = 0;
		newvpos = 0;
	}
	RECT clichild;
	GetClientRect(m_hwnd, &clichild);
	Win32sScrollWindowEx(m_hwnd, m_hScrollPos-newhpos, m_vScrollPos-newvpos,
					NULL, &clichild, NULL, NULL,  SW_INVALIDATE);
								
	m_hScrollPos = newhpos;
	m_vScrollPos = newvpos;
	if (!m_opts.m_FitWindow) {
		UpdateScrollbars();
	} else {
		InvalidateRect(m_hwnd, NULL, FALSE);
	}
	UpdateWindow(m_hwnd);
}

void ClientConnection::CreateLocalFramebuffer() {
	omni_mutex_lock l(m_bitmapdcMutex);

	// Remove old bitmap object if it already exists.
	//
	// The ObjectSelector further down restores the previously selected bitmap
	// when it goes out of scope, so m_hBitmap is not selected into m_hBitmapDC
	// on entry and DeleteObject is safe here.  Clear the member as well: the
	// original code left m_hBitmap pointing at a deleted GDI object across the
	// CreateCompatibleBitmap call, so a failure there left a dangling handle
	// that the destructor then deleted a second time.
	bool bitmapExisted = false;
	if (m_hBitmap != NULL) {
		DeleteObject(m_hBitmap);
		m_hBitmap = NULL;
		bitmapExisted = true;
	}

	// We create a bitmap which has the same pixel characteristics as
	// the local display, in the hope that blitting will be faster.
	
	TempDC hdc(m_hwnd);
	if ((HDC)hdc == NULL)
		throw WarningException("Could not obtain a device context.");

	m_hBitmap = ::CreateCompatibleBitmap(hdc, m_si.framebufferWidth,
										 m_si.framebufferHeight);
	
	// This is the single most likely failure point on a Win32s machine: the
	// framebuffer bitmap for a 1024x768 24-bit remote desktop is over 2 MB of
	// GDI memory, and Win32s GDI is still the 16-bit Windows 3.1 GDI with its
	// limited heap.  Report it as a warning (which closes the connection
	// cleanly) rather than continuing with m_hBitmap == NULL.
	if (m_hBitmap == NULL) {
		vnclog.Print(0, _T("CreateCompatibleBitmap(%d x %d) failed\n"),
					 (int)m_si.framebufferWidth, (int)m_si.framebufferHeight);
		throw WarningException("Error creating local image of screen.\r\n"
							   "The remote desktop may be too large for the "
							   "memory available on this system.");
	}
	
	// Select this bitmap into the DC with an appropriate palette
	ObjectSelector b(m_hBitmapDC, m_hBitmap);
	PaletteSelector p(m_hBitmapDC, m_hPalette);
	
	// Put a "please wait" message up initially
	RECT rect;
	SetRect(&rect, 0,0, m_si.framebufferWidth, m_si.framebufferHeight);
	COLORREF bgcol = RGB(0xcc, 0xcc, 0xcc);
	FillSolidRect(&rect, bgcol);
	
	if (!bitmapExisted) {
		COLORREF oldbgcol  = SetBkColor(m_hBitmapDC, bgcol);
		COLORREF oldtxtcol = SetTextColor(m_hBitmapDC, RGB(0,0,64));
		rect.right = m_si.framebufferWidth / 2;
		rect.bottom = m_si.framebufferHeight / 2;
	
		DrawText (m_hBitmapDC, _T("Please wait - initial screen loading"), -1, &rect,
				  DT_SINGLELINE | DT_CENTER | DT_VCENTER);
		SetBkColor(m_hBitmapDC, oldbgcol);
		SetTextColor(m_hBitmapDC, oldtxtcol);
	}
	
	InvalidateRect(m_hwnd, NULL, FALSE);
}

void ClientConnection::SetupPixelFormat() {
	// Have we requested a reduction to 8-bit?
    if (m_opts.m_Use8Bit) {		
      
		vnclog.Print(2, _T("Requesting 8-bit truecolour\n"));  
		m_myFormat = vnc8bitFormat;
    
		// We don't support colormaps so we'll ask the server to convert
    } else if (!m_si.format.trueColour) {
        
        // We'll just request a standard 16-bit truecolor
        vnclog.Print(2, _T("Requesting 16-bit truecolour\n"));
        m_myFormat = vnc16bitFormat;
        
    } else {

		// Normally we just use the sever's format suggestion
		m_myFormat = m_si.format;

		// WIN32S: never negotiate a 24bpp wire format.  bitsPerPixel 24 means
		// 3 bytes per pixel on the wire, but every decoder below (Raw, Zlib,
		// Hextile, Tight filter selection) only understands 8/16/32, and the
		// old code silently treated 24 as 32 - reading one byte too many per
		// pixel, which garbles Tight and blanks Raw/Zlib.  Promote 24 to
		// padded 32 (same depth and masks); the server pads each pixel and
		// everything downstream works unchanged.
		if (m_myFormat.bitsPerPixel == 24) {
			vnclog.Print(2, _T("Promoting 24bpp server format to 32bpp\n"));
			m_myFormat.bitsPerPixel = 32;
		}

		// It's silly requesting more bits than our current display has, but
		// in fact it doesn't usually amount to much on the network.
		// Windows doesn't support 8-bit truecolour.
		// If our display is palette-based, we want more than 8 bit anyway,
		// unless we're going to start doing palette stuff at the server.
		// So the main use would be a 24-bit true-colour desktop being viewed
		// on a 16-bit true-colour display, and unless you have lots of images
		// and hence lots of raw-encoded stuff, the size of the pixel is not
		// going to make much difference.
		//   We therefore don't bother with any restrictions, but here's the
		// start of the code if we wanted to do it.

		if (false) {
		
			// Get a DC for the root window
			TempDC hrootdc(NULL);
			int localBitsPerPixel = GetDeviceCaps(hrootdc, BITSPIXEL);
			int localRasterCaps	  = GetDeviceCaps(hrootdc, RASTERCAPS);
			vnclog.Print(2, _T("Memory DC has depth of %d and %s pallete-based.\n"), 
				localBitsPerPixel, (localRasterCaps & RC_PALETTE) ? "is" : "is not");
			
			// If we're using truecolor, and the server has more bits than we do
			if ( (localBitsPerPixel > m_myFormat.depth) && 
				! (localRasterCaps & RC_PALETTE)) {
				m_myFormat.depth = localBitsPerPixel;

				// create a bitmap compatible with the current display
				// call GetDIBits twice to get the colour info.
				// set colour masks and shifts
				
			}
		}
	}

	// The endian will be set before sending
}

void ClientConnection::SetFormatAndEncodings()
{
	// Set pixel format to myFormat

	rfbSetPixelFormatMsg spf;

	spf.type = rfbSetPixelFormat;
	spf.format = m_myFormat;
	spf.format.redMax = Swap16IfLE(spf.format.redMax);
	spf.format.greenMax = Swap16IfLE(spf.format.greenMax);
	spf.format.blueMax = Swap16IfLE(spf.format.blueMax);
	spf.format.bigEndian = 0;

	WriteExact((char *)&spf, sz_rfbSetPixelFormatMsg);

	// The number of bytes required to hold at least one pixel.
	m_minPixelBytes = (m_myFormat.bitsPerPixel + 7) >> 3;

	// Set encodings
	//
	// NOTE ON THE BUFFER: buf holds at most MAX_ENCODINGS (20) entries and
	// nothing below checks that bound.  Today the maximum actually written is
	// 9 real encodings + 1 compress level + 3 cursor + 1 quality + 2
	// (LastRect/NewFBSize) = 16, so it fits - but the count depends on
	// LASTENCODING and on user options, so the assumption is worth stating and
	// the writes are now bounded by ADD_ENC below.
	char buf[sz_rfbSetEncodingsMsg + MAX_ENCODINGS * 4];
	rfbSetEncodingsMsg *se = (rfbSetEncodingsMsg *)buf;
	CARD32 *encs = (CARD32 *)(&buf[sz_rfbSetEncodingsMsg]);
	int len = 0;

	se->type = rfbSetEncodings;
	se->nEncodings = 0;

#define ADD_ENC(v) \
	do { \
		if (se->nEncodings < MAX_ENCODINGS) \
			encs[se->nEncodings++] = Swap32IfLE(v); \
	} while (0)

	bool useCompressLevel = false;
	int i;

	// Put the preferred encoding first, and change it if the
	// preferred encoding is not actually usable.
	for (i = LASTENCODING; i >= rfbEncodingRaw; i--)
	{
		if (m_opts.m_PreferredEncoding == i) {
			if (m_opts.m_UseEnc[i]) {
				ADD_ENC(i);
				if ( i == rfbEncodingZlib ||
					 i == rfbEncodingTight ||
					 i == rfbEncodingZlibHex ) {
					useCompressLevel = true;
				}
			} else {
				// Step down to the next encoding.  Clamp at Raw: the loop runs
				// downwards and this could otherwise take m_PreferredEncoding
				// negative, which is then used to index m_UseEnc[] on the next
				// connection.
				if (m_opts.m_PreferredEncoding > rfbEncodingRaw)
					m_opts.m_PreferredEncoding--;
			}
		}
	}

	// Now we go through and put in all the other encodings in order.
	// We do rather assume that the most recent encoding is the most
	// desirable!
	for (i = LASTENCODING; i >= rfbEncodingRaw; i--)
	{
		if ( (m_opts.m_PreferredEncoding != i) &&
			 (m_opts.m_UseEnc[i]))
		{
			ADD_ENC(i);
			if ( i == rfbEncodingZlib ||
				 i == rfbEncodingTight ||
				 i == rfbEncodingZlibHex ) {
				useCompressLevel = true;
			}
		}
	}

	// Request desired compression level if applicable
	if ( useCompressLevel && m_opts.m_useCompressLevel &&
		 m_opts.m_compressLevel >= 0 &&
		 m_opts.m_compressLevel <= 9) {
		ADD_ENC( rfbEncodingCompressLevel0 + m_opts.m_compressLevel );
	}

	// Request cursor shape updates if enabled by user
	if (m_opts.m_requestShapeUpdates) {
		ADD_ENC(rfbEncodingXCursor);
		ADD_ENC(rfbEncodingRichCursor);
		if (!m_opts.m_ignoreShapeUpdates)
			ADD_ENC(rfbEncodingPointerPos);
	}

	// Request JPEG quality level if JPEG compression was enabled by user
	if ( m_opts.m_enableJpegCompression &&
		 m_opts.m_jpegQualityLevel >= 0 &&
		 m_opts.m_jpegQualityLevel <= 9) {
		ADD_ENC( rfbEncodingQualityLevel0 + m_opts.m_jpegQualityLevel );
	}

	// Notify the server that we support LastRect and NewFBSize encodings
	ADD_ENC(rfbEncodingLastRect);
	ADD_ENC(rfbEncodingNewFBSize);

#undef ADD_ENC

	len = sz_rfbSetEncodingsMsg + se->nEncodings * 4;

	se->nEncodings = Swap16IfLE(se->nEncodings);

	WriteExact((char *) buf, len);
}

// Closing down the connection.
// Close the socket and stop servicing the session.
// (Named KillThread() for source compatibility; there is no thread now.)
void ClientConnection::KillThread()
{
	// Set the flags BEFORE closing the socket.  m_running == false is what stops
	// WM_REGIONUPDATED / WM_PAINT / the clipboard handlers from calling
	// WriteExact() again, and those messages can be dispatched between the
	// closesocket() and the next statement if anything below yields.
	m_bKillThread = true;
	m_running = false;
	m_sessionStarted = false;

	if (m_sock != INVALID_SOCKET) {
		SOCKET s = m_sock;
		// Clear the member first, so that a WriteExact() reached from a message
		// dispatched during teardown sees INVALID_SOCKET and returns quietly
		// instead of calling send() on a dying socket and throwing
		// WarningException("WriteExact: Socket error while writing.") out of a
		// window procedure.
		m_sock = INVALID_SOCKET;

		// Graceful close: shutdown(SD_SEND) tells the peer we are done writing
		// and lets the stack flush.  Deliberately NOT SD_BOTH - see the long
		// comment in the destructor about SD_BOTH + closesocket wedging the
		// WinSock 1.1 stack under Win32s.
		shutdown(s, SD_SEND);
		closesocket(s);
	}
}

// Get the RFB options from another connection.
void ClientConnection::CopyOptions(ClientConnection *source)
{
	this->m_opts = source->m_opts;
}

ClientConnection::~ClientConnection()
{
	// Stop servicing the session first.  Order matters on Win32s: DestroyWindow
	// below dispatches WM_DESTROY synchronously, and that handler touches the
	// socket and the clipboard chain.
	m_running = false;
	m_sessionStarted = false;
	m_bKillThread = true;

	if (m_hwnd1 != 0) {
		HWND h = m_hwnd1;
		// Clear the back-pointers BEFORE destroying, so that any message
		// dispatched during destruction sees GWL_USERDATA == 0 and goes to
		// DefWindowProc instead of into a half-destructed object.
		SetWindowLong(h, GWL_USERDATA, (LONG)0);
		if (m_hwndscroll != NULL)
			SetWindowLong(m_hwndscroll, GWL_USERDATA, (LONG)0);
		if (m_hwnd != NULL)
			SetWindowLong(m_hwnd, GWL_USERDATA, (LONG)0);
		m_hwnd1 = 0;
		m_hwnd = 0;
		m_hwndscroll = 0;
		DestroyWindow(h);
	}

	if (m_connDlg != NULL) {
		delete m_connDlg;
		m_connDlg = NULL;
	}

	if (m_sock != INVALID_SOCKET) {
		// NOTE: no shutdown() here any more.
		//
		// shutdown(SD_BOTH) followed by closesocket() is what produced the
		// "WriteExact: Socket error while writing" box on disconnect, and then
		// wedged Windows 3.1 badly enough to need a restart.  The sequence was:
		//
		//   1. WM_CLOSE -> KillThread() already did shutdown + closesocket and
		//      set m_sock = INVALID_SOCKET, OR the peer closed first;
		//   2. some queued work (a WM_REGIONUPDATED repaint asking for the next
		//      update, or the clipboard chain relay) then called WriteExact();
		//   3. send() on a shut-down socket fails, WriteExact throws
		//      WarningException, and that box appears from inside teardown.
		//
		// On a WinSock 1.1 stack under Win32s, calling shutdown() and then
		// closesocket() on a socket the peer has already reset leaves the stack
		// with a half-open connection it never reaps - which is why the machine
		// needed a reboot rather than just showing an error.
		//
		// closesocket() alone is sufficient and is what the stack expects.
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
	}

	if (m_desktopName != NULL) {
		delete [] m_desktopName;
		m_desktopName = NULL;
	}
	if (m_netbuf != NULL) {
		delete [] m_netbuf;
		m_netbuf = NULL;
		m_netbufsize = 0;
	}
	// m_zlibbuf was leaked in the original code; CheckZlibBufferSize() may
	// have allocated it.  Freeing it matters much more on Win32s, where the
	// whole system shares one 16-bit-derived address space.
	if (m_zlibbuf != NULL) {
		delete [] m_zlibbuf;
		m_zlibbuf = NULL;
		m_zlibbufsize = 0;
	}
	if (m_dibbuf != NULL) {
		delete [] m_dibbuf;
		m_dibbuf = NULL;
		m_dibbufsize = 0;
	}
	if (m_pFileTransfer != NULL) {
		delete m_pFileTransfer;
		m_pFileTransfer = NULL;
	}

	// Release the soft-cursor GDI objects (m_hSavedAreaDC/Bitmap and the
	// rcSource/rcMask arrays).  These were never freed on this path.
	SoftCursorFree();

	if (m_hBitmapDC != NULL) {
		DeleteDC(m_hBitmapDC);
		m_hBitmapDC = NULL;
	}
	if (m_hBitmap != NULL) {
		DeleteObject(m_hBitmap);
		m_hBitmap = NULL;
	}
 	if (m_hPalette != NULL) {
 		DeleteObject(m_hPalette);
 		m_hPalette = NULL;
 	}

	DiscardPendingWrites();
 	
 	m_pApp->DeregisterConnection(this);
}

// You can specify a dx & dy outside the limits; the return value will
// tell you whether it actually scrolled.
bool ClientConnection::ScrollScreen(int dx, int dy) 
{
	dx = max(dx, -m_hScrollPos);
	//dx = min(dx, m_hScrollMax-(m_cliwidth-1)-m_hScrollPos);
	dx = min(dx, m_hScrollMax-(m_cliwidth)-m_hScrollPos);
	dy = max(dy, -m_vScrollPos);
	//dy = min(dy, m_vScrollMax-(m_cliheight-1)-m_vScrollPos);
	dy = min(dy, m_vScrollMax-(m_cliheight)-m_vScrollPos);
	if (dx || dy) {
		m_hScrollPos += dx;
		m_vScrollPos += dy;
		RECT clirect;
		GetClientRect(m_hwnd, &clirect);
		Win32sScrollWindowEx(m_hwnd, -dx, -dy,
				NULL, &clirect, NULL, NULL,  SW_INVALIDATE);
		UpdateScrollbars();
		UpdateWindow(m_hwnd);
		return true;
	}
	return false;
}

// Process windows messages
// ==========================================================================
//  Window procedure wrappers.
//
//  Two jobs, both of them things the original code got wrong:
//
//  1. GWL_USERDATA is only set *after* CreateWindow returns, but Windows sends
//     WM_NCCREATE/WM_CREATE/WM_GETMINMAXINFO/WM_NCCALCSIZE during the call.
//     The old procedures dereferenced the resulting NULL '_this' - and on the
//     child/scroll windows a stray WM_HSCROLL or WM_SIZE arriving at the wrong
//     moment did the same.  Every wrapper now bails out to DefWindowProc when
//     _this is NULL.
//
//  2. Exceptions must not escape into USER32.  MSVC 4.1 cannot unwind through
//     the DispatchMessage frame, so a throw from a handler (for example
//     WarningException from the clipboard code, or ErrorException from a decode
//     path) is undefined behaviour rather than an error report.  Each wrapper
//     catches everything and reports it here, on our own stack.
// ==========================================================================

LRESULT CALLBACK ClientConnection::ScrollProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	ClientConnection *_this = (ClientConnection *) GetWindowLong(hwnd, GWL_USERDATA);
	if (_this == NULL)
		return DefWindowProc(hwnd, iMsg, wParam, lParam);

	try {
		return ScrollProcImpl(_this, hwnd, iMsg, wParam, lParam);
	} catch (WarningException &e) {
		e.Report();
	} catch (QuietException &e) {
		e.Report();
	} catch (Exception &e) {
		e.Report();
	}
	return 0;
}

LRESULT ClientConnection::ScrollProcImpl(ClientConnection *_this, HWND hwnd,
										 UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch (iMsg) {
	case WM_HSCROLL:
		{				
			int dx = 0;
			int pos = HIWORD(wParam);
			switch (LOWORD(wParam)) {
			case SB_LINEUP:
				dx = - 2; break;
			case SB_LINEDOWN:
				dx = 2; break;
			case SB_PAGEUP:
				dx = _this->m_cliwidth * -1/4; break;
			case SB_PAGEDOWN:
				dx = _this->m_cliwidth * 1/4; break;
			case SB_THUMBPOSITION:
				dx = pos - _this->m_hScrollPos;
			case SB_THUMBTRACK:
				dx = pos - _this->m_hScrollPos;
			}
			if (!_this->m_opts.m_FitWindow) 
				_this->ScrollScreen(dx,0);
			return 0;
		}
	case WM_VSCROLL:
		{
			int dy = 0;
			int pos = HIWORD(wParam);
			switch (LOWORD(wParam)) {
			case SB_LINEUP:
				dy =  - 2; break;
			case SB_LINEDOWN:
				dy = 2; break;
			case SB_PAGEUP:
				dy =  _this->m_cliheight * -1/4; break;
			case SB_PAGEDOWN:
				dy = _this->m_cliheight * 1/4; break;
			case SB_THUMBPOSITION:
				dy = pos - _this->m_vScrollPos;
			case SB_THUMBTRACK:
				dy = pos - _this->m_vScrollPos;
			}
			if (!_this->m_opts.m_FitWindow) 
				_this->ScrollScreen(0,dy);
			return 0;
		}
	}
	return DefWindowProc(hwnd, iMsg, wParam, lParam);
}
LRESULT CALLBACK ClientConnection::WndProc1(HWND hwnd, UINT iMsg, 
					   WPARAM wParam, LPARAM lParam) 
{
	ClientConnection *_this = (ClientConnection *) GetWindowLong(hwnd, GWL_USERDATA);
	if (_this == NULL)
		return DefWindowProc(hwnd, iMsg, wParam, lParam);

	try {
		return WndProc1Impl(_this, hwnd, iMsg, wParam, lParam);
	} catch (WarningException &e) {
		e.Report();
	} catch (QuietException &e) {
		e.Report();
	} catch (Exception &e) {
		e.Report();
	}
	return 0;
}

LRESULT ClientConnection::WndProc1Impl(ClientConnection *_this, HWND hwnd,
									   UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch (iMsg) {
	
	case WM_NOTIFY:
	{		
		// Only the toolbar sends us WM_NOTIFY, and there is no toolbar when
		// COMCTL32 is unavailable (Win32s).  Guard the pointer as well: a
		// WM_NOTIFY with lParam == 0 would fault on the first dereference.
		LPTOOLTIPTEXT TTStr = (LPTOOLTIPTEXT)lParam;
		if (TTStr == NULL)
			return 0;
		if (TTStr->hdr.code != TTN_NEEDTEXT)
			return 0;

		switch (TTStr->hdr.idFrom) {
		case IDC_OPTIONBUTTON:
			TTStr->lpszText = "Connection options...";
			break;
		case ID_CONN_ABOUT:
			TTStr->lpszText = "Connection info";
			break;
		case ID_FULLSCREEN:
			TTStr->lpszText = "Full screen";
			break;
		case ID_REQUEST_REFRESH:
			TTStr->lpszText = "Request screen refresh";
			break;
		case ID_CONN_CTLALTDEL:
			TTStr->lpszText = "Send Ctrl-Alt-Del";
			break;
		case ID_CONN_CTLESC:
			TTStr->lpszText = "Send Ctrl-Esc";
			break;
		case ID_CONN_CTLDOWN:
			TTStr->lpszText = "Send Ctrl key press/release";
			break;
		case ID_CONN_ALTDOWN:
			TTStr->lpszText = "Send Alt key press/release";
			break;
		case IDD_FILETRANSFER:
			TTStr->lpszText = "Transfer files...";
			break;
		case ID_NEWCONN:
			TTStr->lpszText = "New connection...";
			break;
		case ID_CONN_SAVE_AS:
			TTStr->lpszText = "Save connection info as...";
			break;
		case ID_DISCONNECT:
			TTStr->lpszText = "Disconnect";
			break;
		}
		return 0;
	}
	case WM_SETFOCUS:		
		hotkeys.SetWindow(hwnd);
		SetFocus(_this->m_hwnd);
		return 0;
	case WM_COMMAND:
	case WM_SYSCOMMAND:
		switch (LOWORD(wParam)) {
		case SC_MINIMIZE:
			_this->SetDormant(true);
			break;
		case SC_RESTORE:			
			_this->SetDormant(false);
			break;
		case ID_NEWCONN:
			_this->m_pApp->NewConnection();
			return 0;
		case ID_DISCONNECT:
			SendMessage(hwnd, WM_CLOSE, 0, 0);
			return 0;
		case ID_TOOLBAR:
			if (GetMenuState(GetSystemMenu(_this->m_hwnd1, FALSE),
				ID_TOOLBAR,MF_BYCOMMAND) == MF_CHECKED) {
				CheckMenuItem(GetSystemMenu(_this->m_hwnd1, FALSE),
					ID_TOOLBAR, MF_BYCOMMAND|MF_UNCHECKED);
			} else {
				CheckMenuItem(GetSystemMenu(_this->m_hwnd1, FALSE),
					ID_TOOLBAR, MF_BYCOMMAND|MF_CHECKED);
			}			
			_this->SizeWindow(false);			
			return 0;
		case ID_CONN_SAVE_AS:			
			_this->SaveConnection();
			return 0;			
		case IDC_OPTIONBUTTON:
			{
				// If the options dialog is already open, just raise it.
				// NOTE: the original called SetForegroundWindow(m_hParent)
				// unconditionally - and m_hParent is 0 when no dialog is open.
				// SetForegroundWindow(NULL) returns 0 on NT so the code happened
				// to fall through, but that is accidental.  RaiseDialog() does
				// the check properly (and copes with a stale handle).
				if (_this->m_opts.RaiseDialog())
					return 0;
				int prev_scale_num = _this->m_opts.m_scale_num;
				int prev_scale_den = _this->m_opts.m_scale_den;
				
				if (_this->m_opts.DoDialog(true)) {
					_this->m_pendingFormatChange = true;
					if (_this->m_opts.m_FitWindow) {
						_this->m_opts.m_scaling = true;
						_this->PositionChildWindow();
					} else {
						if (prev_scale_num != _this->m_opts.m_scale_num ||
							prev_scale_den != _this->m_opts.m_scale_den) {
							// Resize the window if scaling factors were changed
							_this->SizeWindow(false);
							InvalidateRect(_this->m_hwnd, NULL, FALSE);
						}
					}
				}
				
				if (_this->m_serverInitiated) {
					_this->m_opts.SaveOpt(".listen", 
										KEY_VNCVIEWER_HISTORI);
				} else {
					_this->m_opts.SaveOpt(_this->m_opts.m_display,
										KEY_VNCVIEWER_HISTORI);
				}
				_this->EnableFullControlOptions();
				return 0;
			}
		case IDD_APP_ABOUT:
			ShowAboutBox();
			return 0;
		case IDD_FILETRANSFER:
			if (_this->m_clientMsgCaps.IsEnabled(rfbFileListRequest)) {
				if (!_this->m_fileTransferDialogShown) {
					_this->m_fileTransferDialogShown = true;
					_this->m_pFileTransfer->CreateFileTransferDialog();
				}
			}
			return 0;
		case ID_CONN_ABOUT:
			_this->ShowConnInfo();
			return 0;
		case ID_FULLSCREEN:
			// Toggle full screen mode
			_this->SetFullScreenMode(!_this->InFullScreenMode());
			return 0;
		case ID_REQUEST_REFRESH: 
			// Request a full-screen update
			_this->SendFullFramebufferUpdateRequest();
			return 0;
		case ID_CONN_CTLESC:
			_this->SendKeyEvent(XK_Control_L, true);
			_this->SendKeyEvent(XK_Escape,     true);
			_this->SendKeyEvent(XK_Escape,     false);
			_this->SendKeyEvent(XK_Control_L, false);
			return 0;
		case ID_CONN_CTLALTDEL:
			_this->SendKeyEvent(XK_Control_L, true);
			_this->SendKeyEvent(XK_Alt_L,     true);
			_this->SendKeyEvent(XK_Delete,    true);
			_this->SendKeyEvent(XK_Delete,    false);
			_this->SendKeyEvent(XK_Alt_L,     false);
			_this->SendKeyEvent(XK_Control_L, false);
			return 0;
		case ID_CONN_CTLDOWN:
			if (GetMenuState(GetSystemMenu(_this->m_hwnd1, FALSE),
				ID_CONN_CTLDOWN, MF_BYCOMMAND) == MF_CHECKED) {
				_this->SendKeyEvent(XK_Control_L, false);
				CheckMenuItem(GetSystemMenu(_this->m_hwnd1, FALSE),
					ID_CONN_CTLDOWN, MF_BYCOMMAND|MF_UNCHECKED);
				if (_this->m_hToolbar != NULL)
					SendMessage(_this->m_hToolbar, TB_SETSTATE, (WPARAM)ID_CONN_CTLDOWN,
					(LPARAM)MAKELONG(TBSTATE_ENABLED, 0));
			} else {
				CheckMenuItem(GetSystemMenu(_this->m_hwnd1, FALSE),
					ID_CONN_CTLDOWN, MF_BYCOMMAND|MF_CHECKED);
				if (_this->m_hToolbar != NULL)
					SendMessage(_this->m_hToolbar, TB_SETSTATE, (WPARAM)ID_CONN_CTLDOWN,
					(LPARAM)MAKELONG(TBSTATE_CHECKED|TBSTATE_ENABLED, 0));
				_this->SendKeyEvent(XK_Control_L, true);
			}
			return 0;
		case ID_CONN_ALTDOWN:
			if(GetMenuState(GetSystemMenu(_this->m_hwnd1, FALSE),
				ID_CONN_ALTDOWN,MF_BYCOMMAND) == MF_CHECKED) {
				_this->SendKeyEvent(XK_Alt_L, false);
				CheckMenuItem(GetSystemMenu(_this->m_hwnd1, FALSE),
					ID_CONN_ALTDOWN, MF_BYCOMMAND|MF_UNCHECKED);
				if (_this->m_hToolbar != NULL)
					SendMessage(_this->m_hToolbar, TB_SETSTATE, (WPARAM)ID_CONN_ALTDOWN,
					(LPARAM)MAKELONG(TBSTATE_ENABLED, 0));
			} else {
				CheckMenuItem(GetSystemMenu(_this->m_hwnd1, FALSE),
					ID_CONN_ALTDOWN, MF_BYCOMMAND|MF_CHECKED);
				if (_this->m_hToolbar != NULL)
					SendMessage(_this->m_hToolbar, TB_SETSTATE, (WPARAM)ID_CONN_ALTDOWN,
					(LPARAM)MAKELONG(TBSTATE_CHECKED|TBSTATE_ENABLED, 0));
				_this->SendKeyEvent(XK_Alt_L, true);
			}
			return 0;
		case ID_CLOSEDAEMON:
			if (MessageBox(NULL, _T("Are you sure you want to exit?"), 
				_T("Closing VNCviewer"), 
				MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES){
				PostQuitMessage(0);
			}
			return 0;
		}
		break;		
	case WM_KILLFOCUS:
		if ( _this->m_opts.m_ViewOnly) return 0;
		_this->SwitchOffKey();
		return 0;
	case WM_SIZE:		
		_this->PositionChildWindow();			
		return 0;	
	case WM_CLOSE:		
		// Stop servicing the session and close the socket.
		//
		// DO NOT try to drain this window's message queue here.
		//
		// An earlier version of this fix looped on
		//     while (PeekMessage(&dmsg, hwnd, 0, 0, PM_REMOVE)) ;
		// to discard messages queued before the disconnect.  That HANGS: a
		// pending WM_PAINT is not removed from the queue by PeekMessage, because
		// the window's update region is only cleared by BeginPaint/EndPaint (or
		// ValidateRect).  PeekMessage keeps handing back the same WM_PAINT
		// forever and the viewer freezes with no way out.
		//
		// The queued-message problem it was trying to solve is handled properly
		// in WriteExact()/ReadExact(), which return silently once m_bKillThread
		// is set - so a WM_REGIONUPDATED or clipboard message dispatched after
		// this point does nothing instead of throwing.
		_this->KillThread();
		DestroyWindow(hwnd);
		return 0;					  
	case WM_DESTROY: 			
#ifndef UNDER_CE
		// Remove us from the clipboard viewer chain.
		// Only if we actually joined it - SetClipboardViewer may have failed,
		// or returned NULL because we were the only viewer.  The declaration
		// was also inside the switch without braces, which MSVC 4.1 accepts
		// but which puts an initialised local in scope for the other labels.
		if (_this->m_hwnd != NULL) {
			ChangeClipboardChain(_this->m_hwnd, _this->m_hwndNextViewer);
			_this->m_hwndNextViewer = NULL;
		}
#endif
		if (_this->m_serverInitiated) {
			_this->m_opts.SaveOpt(".listen", 
								KEY_VNCVIEWER_HISTORI);
		} else {
			_this->m_opts.SaveOpt(_this->m_opts.m_display,
								KEY_VNCVIEWER_HISTORI);
		}
		if (_this->m_waitingOnEmulateTimer) {
			
			KillTimer(_this->m_hwnd, _this->m_emulate3ButtonsTimer);
			_this->m_waitingOnEmulateTimer = false;
		}
			
		_this->m_hwnd1 = 0;
		_this->m_hwnd = 0;
		_this->m_opts.m_hWindow = 0;
		// Single-threaded: there is no worker thread to join.  Flag ourselves
		// for deletion; VNCviewerApp32::ReapDeadConnections() will free this
		// object once we are back in the main message loop.  Deleting here
		// would destroy the object whose window procedure we are inside.
		_this->OnWindowDestroyed();
		return 0;						 
	}
	return DefWindowProc(hwnd, iMsg, wParam, lParam);
}	

LRESULT CALLBACK ClientConnection::Proc(HWND hwnd, UINT iMsg,
										WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hwnd, iMsg, wParam, lParam);
}

LRESULT CALLBACK ClientConnection::WndProc(HWND hwnd, UINT iMsg, 
					   WPARAM wParam, LPARAM lParam) 
{
	ClientConnection *_this = (ClientConnection *) GetWindowLong(hwnd, GWL_USERDATA);
	if (_this == NULL)
		return DefWindowProc(hwnd, iMsg, wParam, lParam);

	try {
		return WndProcImpl(_this, hwnd, iMsg, wParam, lParam);
	} catch (WarningException &e) {
		e.Report();
	} catch (QuietException &e) {
		e.Report();
	} catch (Exception &e) {
		e.Report();
	}
	return 0;
}

LRESULT ClientConnection::WndProcImpl(ClientConnection *_this, HWND hwnd,
									  UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch (iMsg) {
	case WM_REGIONUPDATED:
		// WIN32S: DoBlit() must NOT run here.  It paints with
		// BeginPaint/EndPaint, which is only valid inside WM_PAINT: called
		// from any other message it validates (clears) the update region
		// while painting an empty rcPaint, so the real WM_PAINT that follows
		// finds nothing to draw.  Result was exactly the reported symptom:
		// decoders filled m_hBitmap (control worked, CopyRect BitBlts
		// landed), but the screen stayed blank except when a window move
		// generated a genuine WM_PAINT.  Each decoded rect already calls
		// InvalidateScreenRect(), so the repaint arrives via WM_PAINT below.
		_this->SendAppropriateFramebufferUpdateRequest();
		return 0;
	case WM_PAINT:
		_this->DoBlit();		
		return 0;
	case WM_TIMER:
		if (wParam == _this->m_emulate3ButtonsTimer) {
			_this->SubProcessPointerEvent( 
										_this->m_emulateButtonPressedX,
										 _this->m_emulateButtonPressedY,
										_this->m_emulateKeyFlags);
			KillTimer(hwnd, _this->m_emulate3ButtonsTimer);
			 _this->m_waitingOnEmulateTimer = false;
		}
		return 0; 
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MOUSEMOVE:
	case WM_MOUSEWHEEL:
		{
			if (!_this->m_running)
				return 0;
			if (GetFocus() != hwnd && GetFocus() != _this->m_hwnd1)
				return 0;
			SetFocus(hwnd);

			POINT coords;
			coords.x = LOWORD(lParam);
			coords.y = HIWORD(lParam);

			if (iMsg == WM_MOUSEWHEEL) {
				// Convert coordinates to position in our client area,
				// make sure the pointer is inside the client area.
				if ( WindowFromPoint(coords) != hwnd ||
					 !ScreenToClient(hwnd, &coords) ||
					 coords.x < 0 || coords.y < 0 ||
					 coords.x >= _this->m_cliwidth ||
					 coords.y >= _this->m_cliheight ) {
					return 0;
				}
			} else {
				// Make sure the high-order word in wParam is zero.
				wParam = MAKEWPARAM(LOWORD(wParam), 0);
			}

			if (_this->InFullScreenMode()) {
				if (_this->BumpScroll(coords.x, coords.y))
					return 0;
			}
			if ( _this->m_opts.m_ViewOnly)
				return 0;

			_this->ProcessPointerEvent(coords.x, coords.y, wParam, iMsg);
			return 0;
		}

	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		{
			if (!_this->m_running) return 0;
			if ( _this->m_opts.m_ViewOnly) return 0;
			bool down = (((DWORD) lParam & 0x80000000l) == 0);
			if ((int) wParam == 0x11) {
				if (!down) {
				CheckMenuItem(GetSystemMenu(_this->m_hwnd1, FALSE),
						ID_CONN_CTLDOWN, MF_BYCOMMAND|MF_UNCHECKED);
				if (_this->m_hToolbar != NULL)
					SendMessage(_this->m_hToolbar, TB_SETSTATE, (WPARAM)ID_CONN_CTLDOWN,
						(LPARAM)MAKELONG(TBSTATE_ENABLED, 0));
				} else {
				CheckMenuItem(GetSystemMenu(_this->m_hwnd1, FALSE),
						ID_CONN_CTLDOWN, MF_BYCOMMAND|MF_CHECKED);
				if (_this->m_hToolbar != NULL)
					SendMessage(_this->m_hToolbar, TB_SETSTATE, (WPARAM)ID_CONN_CTLDOWN,
						(LPARAM)MAKELONG(TBSTATE_CHECKED|TBSTATE_ENABLED, 0));
				}
			}
			if ((int) wParam == 0x12) {
				if (!down) {
				CheckMenuItem(GetSystemMenu(_this->m_hwnd1, FALSE),
					ID_CONN_ALTDOWN, MF_BYCOMMAND|MF_UNCHECKED);
				if (_this->m_hToolbar != NULL)
					SendMessage(_this->m_hToolbar, TB_SETSTATE, (WPARAM)ID_CONN_ALTDOWN,
					(LPARAM)MAKELONG(TBSTATE_ENABLED, 0));
				} else {
					CheckMenuItem(GetSystemMenu(_this->m_hwnd1, FALSE),
								ID_CONN_ALTDOWN, MF_BYCOMMAND|MF_CHECKED);
					if (_this->m_hToolbar != NULL)
						SendMessage(_this->m_hToolbar, TB_SETSTATE, (WPARAM)ID_CONN_ALTDOWN,
								(LPARAM)MAKELONG(TBSTATE_CHECKED|TBSTATE_ENABLED, 0));
				}
			}
			
            _this->ProcessKeyEvent((int) wParam, (DWORD) lParam);
			return 0;
		}
	case WM_CHAR:
	case WM_SYSCHAR:
#ifdef UNDER_CE
        {
            int key = wParam;
            vnclog.Print(4,_T("CHAR msg : %02x\n"), key);
            // Control keys which are in the Keymap table will already
            // have been handled.
            if (key == 0x0D  ||  // return
                key == 0x20 ||   // space
                key == 0x08)     // backspace
                return 0;

            if (key < 32) key += 64;  // map ctrl-keys onto alphabet
            if (key > 32 && key < 127) {
                _this->SendKeyEvent(wParam & 0xff, true);
                _this->SendKeyEvent(wParam & 0xff, false);
            }
            return 0;
        }
#endif
	case WM_DEADCHAR:
	case WM_SYSDEADCHAR:
	  return 0;
	case WM_SETFOCUS:
		if (_this->InFullScreenMode())
			SetWindowPos(hwnd, HWND_TOPMOST, 0,0,100,100, SWP_NOMOVE | SWP_NOSIZE);
		return 0;
	// Cacnel modifiers when we lose focus
	case WM_KILLFOCUS:
		{
			if (!_this->m_running) return 0;
			if (_this->InFullScreenMode()) {
				// We must top being topmost, but we want to choose our
				// position carefully.
				HWND foreground = Win32sGetForegroundWindow();
				HWND hwndafter = NULL;
				if ((foreground == NULL) || 
					(GetWindowLong(foreground, GWL_EXSTYLE) & WS_EX_TOPMOST)) {
					hwndafter = HWND_NOTOPMOST;
				} else {
					hwndafter = GetNextWindow(foreground, GW_HWNDNEXT); 
				}

				SetWindowPos(_this->m_hwnd1, hwndafter, 0,0,100,100, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
			}
			vnclog.Print(6, _T("Losing focus - cancelling modifiers\n"));
			return 0;
		}	
    case WM_QUERYNEWPALETTE:
        {
			TempDC hDC(hwnd);
			
			// Select and realize hPalette
			PaletteSelector p(hDC, _this->m_hPalette);
			InvalidateRect(hwnd, NULL, FALSE);
			UpdateWindow(hwnd);

			return TRUE;
        }

	case WM_PALETTECHANGED:
		// If this application did not change the palette, select
		// and realize this application's palette
		if ((HWND) wParam != hwnd)
		{
			// Need the window's DC for SelectPalette/RealizePalette
			TempDC hDC(hwnd);
			PaletteSelector p(hDC, _this->m_hPalette);
			// When updating the colors for an inactive window,
			// UpdateColors can be called because it is faster than
			// redrawing the client area (even though the results are
			// not as good)
				#ifndef UNDER_CE
				UpdateColors(hDC);
				#else
				InvalidateRect(hwnd, NULL, FALSE);
				UpdateWindow(hwnd);
				#endif

		}
        break;

#ifndef UNDER_CE 
		
	case WM_SETCURSOR:
		{
			// if we have the focus, let the cursor change as normal
			if (GetFocus() == hwnd) 
				break;

			// if not, set to default system cursor
			SetCursor( LoadCursor(NULL, IDC_ARROW));
			return 0;
		}

	case WM_DRAWCLIPBOARD:
		_this->ProcessLocalClipboardChange();
		return 0;

	case WM_CHANGECBCHAIN:
		{
			// The clipboard chain is changing
			HWND hWndRemove = (HWND) wParam;     // handle of window being removed 
			HWND hWndNext = (HWND) lParam;       // handle of next window in chain 
			// If next window is closing, update our pointer.
			if (hWndRemove == _this->m_hwndNextViewer)  
				_this->m_hwndNextViewer = hWndNext;  
			// Otherwise, pass the message to the next link.  
			else if (_this->m_hwndNextViewer != NULL) 
				::SendMessage(_this->m_hwndNextViewer, WM_CHANGECBCHAIN, 
				(WPARAM) hWndRemove,  (LPARAM) hWndNext );  
			return 0;
		}
	
#endif
	}

	return DefWindowProc(hwnd, iMsg, wParam, lParam);
}


// ProcessPointerEvent handles the delicate case of emulating 3 buttons
// on a two button mouse, then passes events off to SubProcessPointerEvent.

void
ClientConnection::ProcessPointerEvent(int x, int y, DWORD keyflags, UINT msg) 
{
	if (m_opts.m_Emul3Buttons) {
		// XXX To be done:
		// If this is a left or right press, the user may be 
		// about to press the other button to emulate a middle press.
		// We need to start a timer, and if it expires without any
		// further presses, then we send the button press. 
		// If a press of the other button, or any release, comes in
		// before timer has expired, we cancel timer & take different action.
		if (m_waitingOnEmulateTimer) {
			if (msg == WM_LBUTTONUP || msg == WM_RBUTTONUP ||
				abs(x - m_emulateButtonPressedX) > m_opts.m_Emul3Fuzz ||
				abs(y - m_emulateButtonPressedY) > m_opts.m_Emul3Fuzz) {
				// if button released or we moved too far then cancel.
				// First let the remote know where the button was down
				SubProcessPointerEvent(
					m_emulateButtonPressedX, 
					m_emulateButtonPressedY, 
					m_emulateKeyFlags);
				// Then tell it where we are now
				SubProcessPointerEvent(x, y, keyflags);
			} else if (
				(msg == WM_LBUTTONDOWN && (m_emulateKeyFlags & MK_RBUTTON))
				|| (msg == WM_RBUTTONDOWN && (m_emulateKeyFlags & MK_LBUTTON)))	{
				// Triggered an emulate; remove left and right buttons, put
				// in middle one.
				DWORD emulatekeys = keyflags & ~(MK_LBUTTON|MK_RBUTTON);
				emulatekeys |= MK_MBUTTON;
				SubProcessPointerEvent(x, y, emulatekeys);
				
				m_emulatingMiddleButton = true;
			} else {
				// handle movement normally & don't kill timer.
				// just remove the pressed button from the mask.
				DWORD keymask = m_emulateKeyFlags & (MK_LBUTTON|MK_RBUTTON);
				DWORD emulatekeys = keyflags & ~keymask;
				SubProcessPointerEvent(x, y, emulatekeys);
				return;
			}
			
			// if we reached here, we don't need the timer anymore.
			KillTimer(m_hwnd, m_emulate3ButtonsTimer);
			m_waitingOnEmulateTimer = false;
		} else if (m_emulatingMiddleButton) {
			if ((keyflags & MK_LBUTTON) == 0 && (keyflags & MK_RBUTTON) == 0)
			{
				// We finish emulation only when both buttons come back up.
				m_emulatingMiddleButton = false;
				SubProcessPointerEvent(x, y, keyflags);
			} else {
				// keep emulating.
				DWORD emulatekeys = keyflags & ~(MK_LBUTTON|MK_RBUTTON);
				emulatekeys |= MK_MBUTTON;
				SubProcessPointerEvent(x, y, emulatekeys);
			}
		} else {
			// Start considering emulation if we've pressed a button
			// and the other isn't pressed.
			if ( (msg == WM_LBUTTONDOWN && !(keyflags & MK_RBUTTON))
				|| (msg == WM_RBUTTONDOWN && !(keyflags & MK_LBUTTON)))	{
				// Start timer for emulation.
				m_emulate3ButtonsTimer = 
					SetTimer(
					m_hwnd, 
					IDT_EMULATE3BUTTONSTIMER, 
					m_opts.m_Emul3Timeout, 
					NULL);
				
				if (!m_emulate3ButtonsTimer) {
					// Win32s/Windows 3.1 has a small, system-wide limit on
					// timers and SetTimer genuinely fails when it is reached.
					// Closing the whole connection over a mouse-emulation timer
					// is disproportionate: fall back to sending the button event
					// immediately, i.e. behave as if 3-button emulation were off.
					vnclog.Print(0, _T("No timer available - disabling 3-button emulation\n"));
					m_opts.m_Emul3Buttons = false;
					m_waitingOnEmulateTimer = false;
					SubProcessPointerEvent(x, y, keyflags);
					return;
				}
				
				m_waitingOnEmulateTimer = true;
				
				// Note that we don't send the event here; we're batching it for
				// later.
				m_emulateKeyFlags = keyflags;
				m_emulateButtonPressedX = x;
				m_emulateButtonPressedY = y;
			} else {
				// just send event noramlly
				SubProcessPointerEvent(x, y, keyflags);
			}
		}
	} else {
		SubProcessPointerEvent(x, y, keyflags);
	}
}

// SubProcessPointerEvent takes windows positions and flags and converts 
// them into VNC ones.

inline void
ClientConnection::SubProcessPointerEvent(int x, int y, DWORD keyflags)
{
	int mask;
  
	if (m_opts.m_SwapMouse) {
		mask = ( ((keyflags & MK_LBUTTON) ? rfbButton1Mask : 0) |
				 ((keyflags & MK_MBUTTON) ? rfbButton3Mask : 0) |
				 ((keyflags & MK_RBUTTON) ? rfbButton2Mask : 0) );
	} else {
		mask = ( ((keyflags & MK_LBUTTON) ? rfbButton1Mask : 0) |
				 ((keyflags & MK_MBUTTON) ? rfbButton2Mask : 0) |
				 ((keyflags & MK_RBUTTON) ? rfbButton3Mask : 0) );
	}

	if ((short)HIWORD(keyflags) > 0) {
		mask |= rfbButton4Mask;
	} else if ((short)HIWORD(keyflags) < 0) {
		mask |= rfbButton5Mask;
	}
	
	try {
		int x_scaled =
			(x + m_hScrollPos) * m_opts.m_scale_den / m_opts.m_scale_num;
		int y_scaled =
			(y + m_vScrollPos) * m_opts.m_scale_den / m_opts.m_scale_num;

		SendPointerEvent(x_scaled, y_scaled, mask);

		if ((short)HIWORD(keyflags) != 0) {
			// Immediately send a "button-up" after mouse wheel event.
			mask &= !(rfbButton4Mask | rfbButton5Mask);
			SendPointerEvent(x_scaled, y_scaled, mask);
		}
	} catch (Exception &e) {
		e.Report();
		if (m_hwnd1 != NULL)
			PostMessage(m_hwnd1, WM_CLOSE, 0, 0);
		else
			m_dead = true;
	}
}

//
// SendPointerEvent.
//

inline void
ClientConnection::SendPointerEvent(int x, int y, int buttonMask)
{
    rfbPointerEventMsg pe;

    pe.type = rfbPointerEvent;
    pe.buttonMask = buttonMask;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
	SoftCursorMove(x, y);
    pe.x = Swap16IfLE(x);
    pe.y = Swap16IfLE(y);
	WriteExact((char *)&pe, sz_rfbPointerEventMsg);
}

//
// ProcessKeyEvent
//
// Normally a single Windows key event will map onto a single RFB
// key message, but this is not always the case.  Much of the stuff
// here is to handle AltGr (=Ctrl-Alt) on international keyboards.
// Example cases:
//
//    We want Ctrl-F to be sent as:
//      Ctrl-Down, F-Down, F-Up, Ctrl-Up.
//    because there is no keysym for ctrl-f, and because the ctrl
//    will already have been sent by the time we get the F.
//
//    On German keyboards, @ is produced using AltGr-Q, which is
//    Ctrl-Alt-Q.  But @ is a valid keysym in its own right, and when
//    a German user types this combination, he doesn't mean Ctrl-@.
//    So for this we will send, in total:
//
//      Ctrl-Down, Alt-Down,   
//                 (when we get the AltGr pressed)
//
//      Alt-Up, Ctrl-Up, @-Down, Ctrl-Down, Alt-Down 
//                 (when we discover that this is @ being pressed)
//
//      Alt-Up, Ctrl-Up, @-Up, Ctrl-Down, Alt-Down
//                 (when we discover that this is @ being released)
//
//      Alt-Up, Ctrl-Up
//                 (when the AltGr is released)

inline void ClientConnection::ProcessKeyEvent(int virtkey, DWORD keyData)
{
    bool down = ((keyData & 0x80000000l) == 0);

    // if virtkey found in mapping table, send X equivalent
    // else
    //   try to convert directly to ascii
    //   if result is in range supported by X keysyms,
    //      raise any modifiers, send it, then restore mods
    //   else
    //      calculate what the ascii would be without mods
    //      send that

#ifdef _DEBUG
#ifdef UNDER_CE
	char *keyname = "";
#else
    char keyname[32];
    if (GetKeyNameText(  keyData,keyname, 31)) {
        vnclog.Print(4, _T("Process key: %s (keyData %04x): "), keyname, keyData);
    };
#endif
#endif

	try {
		KeyActionSpec kas = m_keymap.PCtoX(virtkey, keyData);    
		
		if (kas.releaseModifiers & KEYMAP_LCONTROL) {
			SendKeyEvent(XK_Control_L, false );
			vnclog.Print(5, _T("fake L Ctrl raised\n"));
		}
		if (kas.releaseModifiers & KEYMAP_LALT) {
			SendKeyEvent(XK_Alt_L, false );
			vnclog.Print(5, _T("fake L Alt raised\n"));
		}
		if (kas.releaseModifiers & KEYMAP_RCONTROL) {
			SendKeyEvent(XK_Control_R, false );
			vnclog.Print(5, _T("fake R Ctrl raised\n"));
		}
		if (kas.releaseModifiers & KEYMAP_RALT) {
			SendKeyEvent(XK_Alt_R, false );
			vnclog.Print(5, _T("fake R Alt raised\n"));
		}
		
		for (int i = 0; kas.keycodes[i] != XK_VoidSymbol && i < MaxKeysPerKey; i++) {
			SendKeyEvent(kas.keycodes[i], down );
			vnclog.Print(4, _T("Sent keysym %04x (%s)\n"), 
				kas.keycodes[i], down ? _T("press") : _T("release"));
		}
		
		if (kas.releaseModifiers & KEYMAP_RALT) {
			SendKeyEvent(XK_Alt_R, true );
			vnclog.Print(5, _T("fake R Alt pressed\n"));
		}
		if (kas.releaseModifiers & KEYMAP_RCONTROL) {
			SendKeyEvent(XK_Control_R, true );
			vnclog.Print(5, _T("fake R Ctrl pressed\n"));
		}
		if (kas.releaseModifiers & KEYMAP_LALT) {
			SendKeyEvent(XK_Alt_L, false );
			vnclog.Print(5, _T("fake L Alt pressed\n"));
		}
		if (kas.releaseModifiers & KEYMAP_LCONTROL) {
			SendKeyEvent(XK_Control_L, false );
			vnclog.Print(5, _T("fake L Ctrl pressed\n"));
		}
	} catch (Exception &e) {
		e.Report();
		if (m_hwnd1 != NULL)
			PostMessage(m_hwnd1, WM_CLOSE, 0, 0);
		else
			m_dead = true;
	}

}

//
// SendKeyEvent
//

inline void
ClientConnection::SendKeyEvent(CARD32 key, bool down)
{
    rfbKeyEventMsg ke;

    ke.type = rfbKeyEvent;
    ke.down = down ? 1 : 0;
    ke.key = Swap32IfLE(key);
    WriteExact((char *)&ke, sz_rfbKeyEventMsg);
    vnclog.Print(6, _T("SendKeyEvent: key = x%04x status = %s\n"), key, 
        down ? _T("down") : _T("up"));
}

#ifndef UNDER_CE
//
// SendClientCutText
//

void ClientConnection::SendClientCutText(char *str, size_t len)
{
    rfbClientCutTextMsg cct;

    cct.type = rfbClientCutText;
    cct.length = Swap32IfLE(len);
    WriteExact((char *)&cct, sz_rfbClientCutTextMsg);
	WriteExact(str, len);
	vnclog.Print(6, _T("Sent %d bytes of clipboard\n"), len);
}
#endif

// Copy any updated areas from the bitmap onto the screen.

inline void ClientConnection::DoBlit() 
{
	if (m_hBitmap == NULL) return;
	if (!m_running) return;
	// WM_PAINT can be delivered while the window is being torn down.
	if (m_hwnd == NULL) return;
				
	// (No-op lock: single-threaded.  The bitmap DC is only touched from the
	// one thread now, so the previous "no other threads can use bitmap DC"
	// invariant is guaranteed structurally.)
	omni_mutex_lock l(m_bitmapdcMutex);

	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(m_hwnd, &ps);
	if (hdc == NULL)
		return;

	// Select and realize hPalette
	PaletteSelector p(hdc, m_hPalette);
	ObjectSelector b(m_hBitmapDC, m_hBitmap);
			
	if (m_opts.m_delay) {
		// Display the area to be updated for debugging purposes
		COLORREF oldbgcol = SetBkColor(hdc, RGB(0,0,0));
		::ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &ps.rcPaint, NULL, 0, NULL);
		SetBkColor(hdc,oldbgcol);
		::Sleep(m_pApp->m_options.m_delay);
	}
	
	if (m_opts.m_scaling) {
		int n = m_opts.m_scale_num;
		int d = m_opts.m_scale_den;
		
		// We're about to do some scaling on these values in the StretchBlt
		// We want to make sure that they divide nicely by n so we round them
		// down and up appropriately.
		ps.rcPaint.left =   ((ps.rcPaint.left   + m_hScrollPos) / n * n)         - m_hScrollPos;
		ps.rcPaint.right =  ((ps.rcPaint.right  + m_hScrollPos + n - 1) / n * n) - m_hScrollPos;
		ps.rcPaint.top =    ((ps.rcPaint.top    + m_vScrollPos) / n * n)         - m_vScrollPos;
		ps.rcPaint.bottom = ((ps.rcPaint.bottom + m_vScrollPos + n - 1) / n * n) - m_vScrollPos;
		
		// This is supposed to give better results.  I think my driver ignores it?
		SetStretchBltMode(hdc, HALFTONE);
		// The docs say that you should call SetBrushOrgEx after SetStretchBltMode, 
		// but not what the arguments should be.
		Win32sSetBrushOrgEx(hdc, 0,0, NULL);
		
		if (!StretchBlt(
			hdc, 
			ps.rcPaint.left, 
			ps.rcPaint.top, 
			ps.rcPaint.right-ps.rcPaint.left, 
			ps.rcPaint.bottom-ps.rcPaint.top, 
			m_hBitmapDC, 
			(ps.rcPaint.left+m_hScrollPos)     * d / n, 
			(ps.rcPaint.top+m_vScrollPos)      * d / n,
			(ps.rcPaint.right-ps.rcPaint.left) * d / n, 
			(ps.rcPaint.bottom-ps.rcPaint.top) * d / n, 
			SRCCOPY)) 
		{
			vnclog.Print(0, _T("Blit error %d\n"), GetLastError());
			// throw ErrorException("Error in blit!\n");
		};
 	} else {
		// TEMP-DIAG (Raw blank): prove paints run and with what region.
		// Bounded to the first 3 so the log stays small.
		{
			static int s_blitDiag = 0;
			if (s_blitDiag < 3) {
				s_blitDiag++;
				vnclog.Print(0, _T("DIAG DoBlit %d: paint %d,%d %dx%d scroll %d,%d\n"),
							 s_blitDiag,
							 ps.rcPaint.left, ps.rcPaint.top,
							 ps.rcPaint.right - ps.rcPaint.left,
							 ps.rcPaint.bottom - ps.rcPaint.top,
							 m_hScrollPos, m_vScrollPos);
			}
		}
 		if (!BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, 
 			ps.rcPaint.right-ps.rcPaint.left, ps.rcPaint.bottom-ps.rcPaint.top, 
 			m_hBitmapDC, ps.rcPaint.left+m_hScrollPos, ps.rcPaint.top+m_vScrollPos, SRCCOPY)) 
 		{
 			vnclog.Print(0, _T("Blit error %d\n"), GetLastError());
 			// throw ErrorException("Error in blit!\n");
 		}
 	}
 	
 	EndPaint(m_hwnd, &ps);
}

inline void ClientConnection::UpdateScrollbars() 
{
	if (m_hwndscroll == NULL)
		return;

	// We don't update the actual scrollbar info in full-screen mode
	// because it causes them to flicker.
	bool setInfo = !InFullScreenMode();
	if (!setInfo)
		return;

	// SIF_ALL includes SIF_TRACKPOS, which is an *output-only* field for
	// GetScrollInfo and is ignored by SetScrollInfo - harmless, but note it.
	// nTrackPos was never initialised here; zero it so the whole structure is
	// defined (Win32sSetScrollInfo reads fMask and may look at nPage).
	SCROLLINFO scri;
	memset(&scri, 0, sizeof(scri));
	scri.cbSize = sizeof(scri);
	scri.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
	scri.nMin = 0;
	scri.nMax = m_hScrollMax; 
	scri.nPage= m_cliwidth;
	scri.nPos = m_hScrollPos; 
	
	Win32sSetScrollInfo(m_hwndscroll, SB_HORZ, &scri, TRUE);
	
	memset(&scri, 0, sizeof(scri));
	scri.cbSize = sizeof(scri);
	scri.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
	scri.nMin = 0;
	scri.nMax = m_vScrollMax;     
	scri.nPage= m_cliheight;
	scri.nPos = m_vScrollPos; 
	
	Win32sSetScrollInfo(m_hwndscroll, SB_VERT, &scri, TRUE);
}


void ClientConnection::ShowConnInfo()
{
	TCHAR buf[2048];
#ifndef UNDER_CE
	// GetKeyboardLayoutName writes KL_NAMELENGTH (9) bytes including the NUL.
	// Win32sGetKeyboardLayoutName writes "(n/a)" when the API is unavailable.
	char kbdname[16];
	memset(kbdname, 0, sizeof(kbdname));
	Win32sGetKeyboardLayoutName(kbdname);
	kbdname[sizeof(kbdname) - 1] = '\0';
#else
	TCHAR *kbdname = _T("(n/a)");
#endif
	// m_desktopName is server-supplied and bounded to 1024 chars by
	// ReadServerInit; with the fixed text and the numbers this fits 2048, but
	// use _sntprintf so the bound is enforced rather than reasoned about.
	buf[0] = _T('\0');
	_sntprintf(
		buf,
		(sizeof(buf) / sizeof(TCHAR)) - 1,
		_T("Connected to: %s\n\r")
		_T("Host: %s port: %d\n\r\n\r")
		_T("Desktop geometry: %d x %d x %d\n\r")
		_T("Using depth: %d\n\r")
		_T("Current protocol version: 3.%d%s\n\r\n\r")
		_T("Current keyboard name: %s\n\r"),
		m_desktopName, m_host, m_port,
		m_si.framebufferWidth, m_si.framebufferHeight, m_si.format.depth,
		m_myFormat.depth,
		m_minorVersion, (m_tightVncProtocol ? "tight" : ""),
		kbdname);
	buf[(sizeof(buf) / sizeof(TCHAR)) - 1] = _T('\0');
	MessageBox(m_hwnd1, buf, _T("VNC connection info"), MB_ICONINFORMATION | MB_OK);
}

// ********************************************************************
//  Methods after this point used to run on the per-connection worker
//  thread.  They now run on the single application thread, driven by
//  PumpIdle() below.
// ********************************************************************

//
// SocketHasData - non-blocking test for "is there at least one byte to read?"
//
// select() with a zero timeout is the one polling primitive that behaves the
// same on WinSock 1.1 under Win32s/Win 3.1 (winsock.dll / trumpwsk / MS TCP)
// as it does on Win9x and NT.  We deliberately do NOT use recv(MSG_PEEK) as
// the loop condition any more: on a blocking socket recv() will block if no
// data has arrived, and with a single thread that freezes the whole viewer.
//
bool ClientConnection::SocketHasData()
{
	if (m_sock == INVALID_SOCKET)
		return false;

	fd_set fds;
	struct timeval tv;

	FD_ZERO(&fds);
	FD_SET(m_sock, &fds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	// nfds is ignored by WinSock (the fd_set is an array of handles, not a
	// bitmask), so 0 is correct and portable across stacks.
	int res = select(0, &fds, NULL, NULL, &tv);
	if (res == SOCKET_ERROR) {
		int err = WSAGetLastError();
		// A real error here means the connection is gone.  Mark it so the main
		// loop stops polling us; do not throw from the polling path.
		if (err != WSAEINTR && err != WSAEINPROGRESS) {
			vnclog.Print(2, _T("select() failed: %d - ending session\n"), err);
			m_running = false;
			m_bKillThread = true;
			if (m_hwnd1 != NULL)
				PostMessage(m_hwnd1, WM_CLOSE, 0, 0);
			else
				m_dead = true;
		}
		return false;
	}

	return (res > 0);
}

//
// PumpIdle - service at most one server message.
//
// This is the single-threaded replacement for the body of the old
// run_undetached() while-loop.  Contract:
//   * never blocks: returns false immediately if nothing is pending
//   * returns true if it processed something, so the caller can pump again
//     before going back to sleep in GetMessage()
//   * on protocol/socket failure it closes the window exactly as the worker
//     thread used to, and marks the object dead if there is no window
//
bool ClientConnection::PumpIdle()
{
	if (!m_sessionStarted || m_bKillThread || m_dead)
		return false;

	if (m_sock == INVALID_SOCKET)
		return false;

	if (!SocketHasData())
		return false;

	try {
		// Look at the type of the message, but leave it in the buffer.
		// Safe here because select() has already told us a byte is waiting.
		CARD8 msgType;
		int bytes = recv(m_sock, (char *) &msgType, 1, MSG_PEEK);
		if (bytes == 0) {
			m_pFileTransfer->CloseUndoneFileTransfers();
			vnclog.Print(0, _T("Connection closed\n"));
			throw WarningException(_T("Connection closed"));
		}
		if (bytes < 0) {
			int err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK)
				return false;			// spurious wakeup, try again later
			m_pFileTransfer->CloseUndoneFileTransfers();
			vnclog.Print(3, _T("Socket error reading message: %d\n"), err);
			throw WarningException("Error while waiting for server message");
		}

		switch (msgType) {
		case rfbFramebufferUpdate:
			ReadScreenUpdate();
			break;
		case rfbSetColourMapEntries:
			ReadSetColourMapEntries();
			break;
		case rfbBell:
			ReadBell();
			break;
		case rfbServerCutText:
			ReadServerCutText();
			break;
		case rfbFileListData:
			m_pFileTransfer->ShowServerItems();
			break;
		case rfbFileDownloadData:
			m_pFileTransfer->FileTransferDownload();
			break;
		case rfbFileUploadCancel:
			m_pFileTransfer->ReadUploadCancel();
			break;
		case rfbFileDownloadFailed:
			m_pFileTransfer->ReadDownloadFailed();
			break;

		default:
			vnclog.Print(3, _T("Unknown message type x%02x\n"), msgType);
			throw WarningException("Unhandled message type received!\n");
		}

		return true;

	} catch (WarningException &e) {
		// The original worker loop tested "if (!m_bKillThread)" before
		// reporting, so that a disconnection we asked for is silent while an
		// unexpected one is reported.  Sample the flag BEFORE setting it -
		// setting it first (as an obvious transcription of that code would)
		// suppresses every message, including real errors.
		bool expected = m_bKillThread;

		m_running = false;
		m_bKillThread = true;
		if (m_hwnd1 != NULL) {
			PostMessage(m_hwnd1, WM_CLOSE, 0, 0);
		} else {
			m_dead = true;
		}
		if (!expected) {
			e.Report();
		}
	} catch (QuietException &e) {
		m_running = false;
		m_bKillThread = true;
		e.Report();
		if (m_hwnd1 != NULL) {
			PostMessage(m_hwnd1, WM_CLOSE, 0, 0);
		} else {
			m_dead = true;
		}
	} catch (Exception &e) {
		// ErrorException and anything else: PumpIdle is called from the main
		// loop, and an escape from here would unwind out of WinMain.
		m_running = false;
		m_bKillThread = true;
		e.Report();
		if (m_hwnd1 != NULL) {
			PostMessage(m_hwnd1, WM_CLOSE, 0, 0);
		} else {
			m_dead = true;
		}
	}

	return false;
}

//
// OnWindowDestroyed - called from WndProc1 on WM_DESTROY.
//
// The old code called join() here to wait for the worker thread, and the
// comment noted "after joining, _this is no longer valid" - join() deleted the
// object from inside a window procedure.  With one thread there is nothing to
// join, and deleting the object here would destroy the very window Windows is
// currently dispatching to.  Instead we just flag ourselves; the app deletes
// us from ReapDeadConnections() once control is back in the message loop.
//
void ClientConnection::OnWindowDestroyed()
{
	m_running = false;
	m_bKillThread = true;
	m_sessionStarted = false;
	m_dead = true;
}


//
// Requesting screen updates from the server
//

inline void
ClientConnection::SendFramebufferUpdateRequest(int x, int y, int w, int h, bool incremental)
{
    rfbFramebufferUpdateRequestMsg fur;

    fur.type = rfbFramebufferUpdateRequest;
    fur.incremental = incremental ? 1 : 0;
    fur.x = Swap16IfLE(x);
    fur.y = Swap16IfLE(y);
    fur.w = Swap16IfLE(w);
    fur.h = Swap16IfLE(h);

	vnclog.Print(10, _T("Request %s update\n"), incremental ? _T("incremental") : _T("full"));
    WriteExact((char *)&fur, sz_rfbFramebufferUpdateRequestMsg);
}

inline void ClientConnection::SendIncrementalFramebufferUpdateRequest()
{
    SendFramebufferUpdateRequest(0, 0, m_si.framebufferWidth,
					m_si.framebufferHeight, true);
}

inline void ClientConnection::SendFullFramebufferUpdateRequest()
{
    SendFramebufferUpdateRequest(0, 0, m_si.framebufferWidth,
					m_si.framebufferHeight, false);
}


void ClientConnection::SendAppropriateFramebufferUpdateRequest()
{
	// Called from WM_REGIONUPDATED, so it can fire once more after the session
	// has ended (the message is already in the queue when the socket dies).
	// WriteExact would then run on INVALID_SOCKET; it checks, but the format
	// renegotiation below does real work first.
	if (!m_running || m_sock == INVALID_SOCKET)
		return;

	if (m_pendingFormatChange) {
		vnclog.Print(3, _T("Requesting new pixel format\n") );
		rfbPixelFormat oldFormat = m_myFormat;
		SetupPixelFormat();
		SoftCursorFree();
		SetFormatAndEncodings();
		m_pendingFormatChange = false;
		// If the pixel format has changed, request whole screen
		if (!PF_EQ(m_myFormat, oldFormat)) {
			SendFullFramebufferUpdateRequest();	
		} else {
			SendIncrementalFramebufferUpdateRequest();
		}
	} else {
		if (!m_dormant)
			SendIncrementalFramebufferUpdateRequest();
	}
}


// A ScreenUpdate message has been received

void ClientConnection::ReadScreenUpdate() {

	rfbFramebufferUpdateMsg sut;
	ReadExact((char *) &sut, sz_rfbFramebufferUpdateMsg);
    sut.nRects = Swap16IfLE(sut.nRects);
	if (sut.nRects == 0) return;
	
	for (int i=0; i < sut.nRects; i++) {

		rfbFramebufferUpdateRectHeader surh;
		ReadExact((char *) &surh, sz_rfbFramebufferUpdateRectHeader);

		surh.encoding = Swap32IfLE(surh.encoding);
		surh.r.x = Swap16IfLE(surh.r.x);
		surh.r.y = Swap16IfLE(surh.r.y);
		surh.r.w = Swap16IfLE(surh.r.w);
		surh.r.h = Swap16IfLE(surh.r.h);

		if (surh.encoding == rfbEncodingLastRect)
			break;
		if (surh.encoding == rfbEncodingNewFBSize) {
			ReadNewFBSize(&surh);
			break;
		}

		if ( surh.encoding == rfbEncodingXCursor ||
			 surh.encoding == rfbEncodingRichCursor ) {
			ReadCursorShape(&surh);
			continue;
		}

		if (surh.encoding == rfbEncodingPointerPos) {
			ReadCursorPos(&surh);
			continue;
		}

		// Validate the rectangle against our framebuffer before handing it to a
		// decoder.  Every decoder writes into m_hBitmap via SETPIXELS/BitBlt
		// using these coordinates, and none of them re-check.  A rectangle that
		// extends past the framebuffer is a straightforward out-of-bounds draw,
		// and CopyRect additionally uses it as a *source*.
		if ((int)surh.r.x < 0 || (int)surh.r.y < 0 ||
			(int)surh.r.w < 0 || (int)surh.r.h < 0 ||
			(int)surh.r.x + (int)surh.r.w > (int)m_si.framebufferWidth ||
			(int)surh.r.y + (int)surh.r.h > (int)m_si.framebufferHeight) {
			vnclog.Print(0, _T("Bad update rectangle %d,%d %dx%d (fb %dx%d)\n"),
						 (int)surh.r.x, (int)surh.r.y,
						 (int)surh.r.w, (int)surh.r.h,
						 (int)m_si.framebufferWidth, (int)m_si.framebufferHeight);
			throw ErrorException("Protocol error: update rectangle outside the framebuffer.");
		}

		// If *Cursor encoding is used, we should prevent collisions
		// between framebuffer updates and cursor drawing operations.
		SoftCursorLockArea(surh.r.x, surh.r.y, surh.r.w, surh.r.h);

		switch (surh.encoding) {
		case rfbEncodingRaw:
			ReadRawRect(&surh);
			break;
		case rfbEncodingCopyRect:
			ReadCopyRect(&surh);
			break;
		case rfbEncodingRRE:
			ReadRRERect(&surh);
			break;
		case rfbEncodingCoRRE:
			ReadCoRRERect(&surh);
			break;
		case rfbEncodingHextile:
			ReadHextileRect(&surh);
			break;
		case rfbEncodingZlib:
			ReadZlibRect(&surh);
			break;
		case rfbEncodingTight:
			ReadTightRect(&surh);
			break;
		case rfbEncodingZlibHex:
			ReadZlibHexRect(&surh);
			break;
		default:
			vnclog.Print(0, _T("Unknown encoding %d - not supported!\n"), surh.encoding);
			break;
		}

		// Tell the system to update a screen rectangle. Note that
		// InvalidateScreenRect member function knows about scaling.
		RECT rect;
		SetRect(&rect, surh.r.x, surh.r.y,
				surh.r.x + surh.r.w, surh.r.y + surh.r.h);
		InvalidateScreenRect(&rect);

		// Now we may discard "soft cursor locks".
		SoftCursorUnlockScreen();
	}	

	// Ask the window to repaint the area we just decoded.
	//
	// (Was "inform the other thread"; there is one thread now.)  Keep it a
	// PostMessage rather than calling DoBlit() directly: we are inside
	// PumpIdle() here, and a synchronous repaint from the middle of a decode
	// would re-enter the GDI state that the decoders are using.  The main loop
	// picks the message up on its next pass.
	if (m_hwnd != NULL)
		PostMessage(m_hwnd, WM_REGIONUPDATED, 0, 0);
}	

void ClientConnection::SetDormant(bool newstate)
{
	vnclog.Print(5, _T("%s dormant mode\n"), newstate ? _T("Entering") : _T("Leaving"));
	m_dormant = newstate;
	if (!m_dormant)
		SendIncrementalFramebufferUpdateRequest();
}

// The server has copied some text to the clipboard - put it 
// in the local clipboard too.

void ClientConnection::ReadServerCutText()
{
	rfbServerCutTextMsg sctm;
	vnclog.Print(6, _T("Read remote clipboard change\n"));
	ReadExact((char *) &sctm, sz_rfbServerCutTextMsg);
	size_t len = Swap32IfLE(sctm.length);

	// CheckBufferSize now rejects absurd sizes (see its Win32s note), but bound
	// the clipboard specifically: UpdateLocalClipboard allocates len*2+1 on top
	// of this, and GlobalAlloc on Win32s draws from the shared 16-bit heap.
	if (len > 0x00100000) {			// 1 MB of clipboard text is already absurd
		vnclog.Print(0, _T("Server sent %u bytes of clipboard text - ignoring\n"),
					 (unsigned int)len);
		// Still have to consume it to stay in sync with the protocol stream.
		char discard[1024];
		while (len > 0) {
			int chunk = (len > sizeof(discard)) ? sizeof(discard) : (int)len;
			ReadExact(discard, chunk);
			len -= chunk;
		}
		return;
	}

	CheckBufferSize(len);
	if (len == 0) {
		m_netbuf[0] = '\0';
	} else {
		ReadString(m_netbuf, len);
	}
	UpdateLocalClipboard(m_netbuf, len);
}

void ClientConnection::ReadSetColourMapEntries()
{
	// Currently, we read and silently ignore SetColourMapEntries.

	rfbSetColourMapEntriesMsg msg;
	vnclog.Print(3, _T("Read server colour map entries (ignored)\n"));
	ReadExact((char *)&msg, sz_rfbSetColourMapEntriesMsg);
	int numEntries = Swap16IfLE(msg.nColours);

	if (numEntries > 0) {
		size_t nBytes = 6 * numEntries;
		CheckBufferSize(nBytes);
		ReadExact(m_netbuf, nBytes);
	}
}

void ClientConnection::ReadBell() {
	rfbBellMsg bm;
	ReadExact((char *) &bm, sz_rfbBellMsg);

	#ifdef UNDER_CE
	MessageBeep( MB_OK );
	#else

	// Was PlaySound(... SND_APPLICATION|SND_ALIAS ...) plus Beep().  Both are
	// problems on Win32s: PlaySound is a winmm import that resolves a registry
	// sound scheme Windows 3.1 does not have, and Beep() is a no-op/absent on
	// several Win32s builds.  Win32sPlayBell() uses MessageBeep, which works
	// everywhere and needs no extra import.
	Win32sPlayBell();
	#endif
	if (m_opts.m_DeiconifyOnBell && m_hwnd1 != NULL) {
		if (IsIconic(m_hwnd1)) {
			SetDormant(false);
			ShowWindow(m_hwnd1, SW_SHOWNORMAL);
		}
	}
	vnclog.Print(6, _T("Bell!\n"));
}


// General utilities -------------------------------------------------

// Reads the number of bytes specified into the buffer given.
//
// NOTE (single-threaded): this still blocks until the requested bytes arrive.
// That is safe because PumpIdle() only calls into the protocol code after
// select() has reported readable data, so we are always completing a message
// that the server has already started sending.  Do not call ReadExact()
// speculatively from the idle loop.
void ClientConnection::ReadExact(char *inbuf, int wanted)
{
	// Was "if (m_sock == INVALID_SOCKET && m_bKillThread)": the && meant that a
	// read attempted on a closed socket while NOT shutting down fell through to
	// recv(INVALID_SOCKET, ...).  Either condition alone means we must not read.
	if (m_sock == INVALID_SOCKET || m_bKillThread)
		throw QuietException("Connection closed.");
	if (inbuf == NULL || wanted < 0)
		throw QuietException("Bad read request.");
	if (wanted == 0)
		return;

	omni_mutex_lock l(m_readMutex);

	if (m_inWriteExact) {
		static bool logged = false;
		if (!logged) {
			logged = true;
			vnclog.Print(0, _T("ReadExact during WriteExact: OS dispatched input mid-write\n"));
		}
	}
	m_inReadExact = true;

	int offset = 0;
	int wouldBlockRetries = 0;
    vnclog.Print(10, _T("  reading %d bytes\n"), wanted);
	
	while (wanted > 0) {

		// WIN32S: bound every recv() to 16K.  The 16-bit Winsock 1.1 thunk
		// cannot marshal a large flat buffer (a full-screen Raw rect is
		// megabytes in one call) and fails it with WSAEFAULT, which used to
		// surface as an immediate disconnect on Raw/RRE and as random stalls
		// on any big update.  Small reads were always fine - hence Tight and
		// small Hextile tiles working while Raw never did.  Same root cause
		// as the server-side send cap (winvnc VSocket::SendFromQueue).
		int chunk = (wanted > 16384) ? 16384 : wanted;
		int bytes = recv(m_sock, inbuf+offset, chunk, 0);
		if (bytes == 0) { m_inReadExact = false; DiscardPendingWrites(); throw WarningException("Connection closed."); }
		if (bytes == SOCKET_ERROR) {
			// Must be WSAGetLastError(), not GetLastError(): under Win32s the
			// WinSock DLL does not funnel socket errors into the Win32 thread
			// error value, so GetLastError() returns garbage here.
			int err = WSAGetLastError();

		// See WriteExact: some WinSock 1.1 stacks return WSAEWOULDBLOCK
		// transiently even on a blocking socket, and WSAEINPROGRESS while the
		// stack is still draining a previous bulk transfer.  This matters much
		// more here, mid-message, than it does for writes: dropping out would
		// desynchronise the protocol stream.  Bounded, and waits with
		// select() rather than Sleep() - see the note in WriteExact about
		// Sleep(0) not yielding on Win32s.
		if ((err == WSAEWOULDBLOCK || err == WSAEINPROGRESS) &&
			wouldBlockRetries < 200) {
				wouldBlockRetries++;
				fd_set rfds;
				struct timeval rtv;
				FD_ZERO(&rfds);
				FD_SET(m_sock, &rfds);
				rtv.tv_sec = 0;
				rtv.tv_usec = 50000;		// 50 ms
				select(0, &rfds, NULL, NULL, &rtv);
				continue;
			}

 		vnclog.Print(1, _T("Socket error while reading %d\n"), err);
 		m_running = false;
 		m_inReadExact = false;
		DiscardPendingWrites();
 		if (m_bKillThread)
 			throw QuietException("Connection closed.");
 			m_bKillThread = true;
 			throw WarningException("ReadExact: Socket error while reading.");
		}
 		wanted -= bytes;
 		offset += bytes;
 
 	}
 	m_inReadExact = false;
	// A full-screen Raw rect keeps recv() busy long enough for the OS to
	// dispatch input mid-read; those writes were queued above - send them
	// now that the socket is free, preserving order.
	FlushPendingWrites();
 }

// Read the number of bytes and return them zero terminated in the buffer 
inline void ClientConnection::ReadString(char *buf, int length)
{
	if (length > 0)
		ReadExact(buf, length);
	buf[length] = '\0';
    vnclog.Print(10, _T("Read a %d-byte string\n"), length);
}


void ClientConnection::DiscardPendingWrites()
{
	PendingWrite *pw = m_pendingHead;
	m_pendingHead = NULL;
	m_pendingTail = NULL;
	while (pw != NULL) {
		PendingWrite *next = pw->next;
		if (pw->data != NULL)
			delete [] pw->data;
		delete pw;
		pw = next;
	}
}

// Sends everything WriteExact() deferred during the last ReadExact, in FIFO
// order.  Runs with m_inReadExact already false, so the socket is free; each
// item uses the same 16K-chunked send loop as WriteExact.
void ClientConnection::FlushPendingWrites()
{
	if (m_pendingHead == NULL)
		return;
	// Never flush into teardown: WriteExact itself stays silent there.
	if (m_sock == INVALID_SOCKET || m_bKillThread) {
		DiscardPendingWrites();
		return;
	}
	// Flag the drain: a send() below can itself dispatch input on Win32s, and
	// the re-entrant WriteExact must enqueue (see below) rather than send on
	// the busy socket.  The while() re-checks the head, so items queued
	// mid-flush are picked up by this same drain.
	m_inWriteExact = true;
	while (m_pendingHead != NULL) {
		PendingWrite *pw = m_pendingHead;
		m_pendingHead = pw->next;
		if (m_pendingHead == NULL)
			m_pendingTail = NULL;
		int i = 0;
		int wouldBlockRetries = 0;
		while (i < pw->len) {
			int chunk = pw->len - i;
			if (chunk > 16384)
				chunk = 16384;
			int j = send(m_sock, pw->data + i, chunk, 0);
			if (j == SOCKET_ERROR || j == 0) {
				int err = WSAGetLastError();
				if ((err == WSAEWOULDBLOCK || err == WSAEINPROGRESS) &&
					wouldBlockRetries < 64) {
					wouldBlockRetries++;
					fd_set wfds;
					struct timeval wtv;
					FD_ZERO(&wfds);
					FD_SET(m_sock, &wfds);
					wtv.tv_sec = 0;
					wtv.tv_usec = 50000;
					select(0, NULL, &wfds, NULL, &wtv);
					continue;
				}
 				vnclog.Print(1, _T("Socket error %d while writing deferred data\n"), err);
 				if (pw->data != NULL)
 					delete [] pw->data;
 				delete pw;
 				DiscardPendingWrites();
 				m_running = false;
 				m_bKillThread = true;
				m_inWriteExact = false;
 				throw WarningException("WriteExact: Socket error while writing.");
 			}
 			i += j;
 		}
 		if (pw->data != NULL)
 			delete [] pw->data;
 		delete pw;
 	}
	m_inWriteExact = false;
}

// Sends the number of bytes specified from the buffer
inline void ClientConnection::WriteExact(char *buf, int bytes)
{
	// Do nothing once the session is over.
	//
	// This is the fix for the "WriteExact: Socket error while writing" box on
	// disconnect.  The old guard tested only m_sock, but there is a window
	// during teardown in which m_sock is still valid and the peer has already
	// gone: WM_CLOSE -> KillThread() runs, and any message still in the queue
	// (WM_REGIONUPDATED asking for the next framebuffer update, WM_DRAWCLIPBOARD
	// relaying a clipboard change, a queued key/mouse event) is dispatched
	// afterwards and calls in here.  send() then fails, this threw
	// WarningException from inside a window procedure, and the user got a modal
	// error box during shutdown.
	//
	// Testing m_bKillThread as well makes the whole shutdown path silent, which
	// is what the original multi-threaded code achieved with its
	// "if (!m_bKillThread) e.Report()" check in the worker loop.
	if (bytes == 0 || m_sock == INVALID_SOCKET || m_bKillThread)
		return;
	if (buf == NULL || bytes < 0)
		return;

  	// WIN32S: never touch the socket while it is busy.  Win32s dispatches
	// window input from inside a blocking recv() - and also from inside a
	// blocking send() - so this WriteExact may itself have been dispatched
	// mid-read or mid-write; a re-entrant send() draws WSAEINPROGRESS and,
	// after retries, kills the session (10036 -> 10038 in the logs).
	// Enqueue a copy instead; the outer ReadExact (or WriteExact, or flush)
	// drains it in order once the socket is free.  Logged once per session.
  	if (m_inReadExact || m_inWriteExact) {
 		static bool logged = false;
 		if (!logged) {
 			logged = true;
 			vnclog.Print(0, _T("WriteExact during ReadExact: deferring %d bytes\n"), bytes);
 		}
		PendingWrite *pw = new PendingWrite;
		if (pw == NULL)
			throw WarningException("WriteExact: out of memory queuing deferred write.");
		pw->data = new char[bytes];
		if (pw->data == NULL) {
			delete pw;
			throw WarningException("WriteExact: out of memory queuing deferred write.");
		}
		memcpy(pw->data, buf, bytes);
		pw->len = bytes;
		pw->next = NULL;
		if (m_pendingTail != NULL)
			m_pendingTail->next = pw;
		else
			m_pendingHead = pw;
		m_pendingTail = pw;
 		return;
 	}
 
 	omni_mutex_lock l(m_writeMutex);
 	vnclog.Print(10, _T("  writing %d bytes\n"), bytes);
 
 	m_inWriteExact = true;

	int i = 0;
    int j;
	int wouldBlockRetries = 0;

    while (i < bytes) {

		// WIN32S: bound every send() to 16K, mirroring ReadExact above and
		// the server-side send cap.  A single large send (clipboard bulk,
		// file-transfer blocks) faults the 16-bit thunk with WSAEFAULT and
		// used to surface as the random "WriteExact: Socket error" box.
		int chunk = bytes - i;
		if (chunk > 16384)
			chunk = 16384;
		j = send(m_sock, buf+i, chunk, 0);
		if (j == SOCKET_ERROR || j==0) {
			// Was: GetLastError() + FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER)
			// + LocalFree().  Two problems on Win32s: socket errors do not
			// appear in GetLastError(), and the allocate-buffer form of
			// FormatMessage is unreliable there.  Log the WinSock error code.
			int err = WSAGetLastError();

		// WSAEWOULDBLOCK should not happen on a blocking socket, but some
		// WinSock 1.1 stacks return it transiently when their send buffer is
		// full.  WSAEINPROGRESS is retried for the same reason: after a bulk
		// transfer the 16-bit stack has been observed to report the socket
		// busy once or twice before accepting the next call (a send issued
		// while the stack is still draining).  Retry a BOUNDED number of times.
		//
		// The retry must be bounded and must yield properly: Sleep(0) on
		// Win32s does not give other tasks a chance to run (scheduling is
		// cooperative and Sleep is close to a no-op), so an unbounded
		// "Sleep(0); continue;" spins the one thread forever and freezes the
		// whole system - which is exactly the failure mode this function is
		// supposed to be preventing.
		if ((err == WSAEWOULDBLOCK || err == WSAEINPROGRESS) &&
			wouldBlockRetries < 64) {
			wouldBlockRetries++;
			// select() for writability with a short timeout: this both waits
			// and yields, unlike Sleep().
			fd_set wfds;
			struct timeval wtv;
			FD_ZERO(&wfds);
			FD_SET(m_sock, &wfds);
			wtv.tv_sec = 0;
			wtv.tv_usec = 50000;		// 50 ms
			select(0, NULL, &wfds, NULL, &wtv);
			continue;
		}

		vnclog.Print(1, _T("Socket error %d while writing\n"), err);
		m_running = false;

		// Mark the session dead and stay quiet if we are already shutting
		// down; otherwise report as before.
		if (m_bKillThread) {
			m_inWriteExact = false;
			return;
		}

 		m_bKillThread = true;
 		m_inWriteExact = false;
 		throw WarningException("WriteExact: Socket error while writing.");
 		}
 		i += j;
     }
 	m_inWriteExact = false;
	// Drain anything dispatched (and deferred above) while this send held
	// the socket.  No-op when nothing was queued.
	FlushPendingWrites();
 }

// Read the string describing the reason for a connection failure.
// This function reads the data into m_netbuf, and returns that pointer
// as the beginning of the reason string.
char *ClientConnection::ReadFailureReason()
{
	CARD32 reasonLen;
	ReadExact((char *)&reasonLen, sizeof(reasonLen));
	reasonLen = Swap32IfLE(reasonLen);

	// A failing server is exactly the situation in which the length field is
	// least trustworthy, and this string is handed to WarningException, which
	// copies it with new[]/strcpy.  Cap it.
	if (reasonLen > 1024)
		reasonLen = 1024;

	CheckBufferSize(reasonLen + 1);
	ReadString(m_netbuf, reasonLen);

	// Sanitise: this goes into a message box.
	for (CARD32 ri = 0; ri < reasonLen; ri++) {
		unsigned char c = (unsigned char)m_netbuf[ri];
		if (c < 0x20 && c != '\r' && c != '\n' && c != '\t')
			m_netbuf[ri] = ' ';
	}

	vnclog.Print(0, _T("RFB connection failed, reason: %s\n"), m_netbuf);
	return m_netbuf;
}

// ==========================================================================
//  BULK PIXEL DRAWING - the fix for "loading the screen takes forever"
// ==========================================================================
//
//  The decoders used to draw through the SETPIXELS / SETPIXELS_NOCONV macros in
//  ClientConnection.h, which expand to a nested loop calling SetPixel() (or
//  SetPixelV()) once per pixel.  A 640x480 full-screen update is 307,200 GDI
//  calls.
//
//  On Win32s every one of those is a 32->16 bit thunk into the 16-bit GDI, and
//  the thunk layer - not the drawing - dominates: each call has to marshal
//  arguments across the boundary, and Win32s cannot batch them.  That is why the
//  first screen takes tens of seconds, and why Hextile "works best": Hextile
//  paints most tiles with FillSolidRect (one ExtTextOut per rectangle) and only
//  falls back to per-pixel work for the raw sub-rectangles, so it makes orders of
//  magnitude fewer calls than Raw or Zlib do.
//
//  These two functions build a bottom-up 24-bit DIB from the decoded pixels and
//  hand the whole rectangle to GDI in one SetDIBitsToDevice call.  That is one
//  thunk per rectangle instead of one per pixel.
//
//  Notes:
//   * SetDIBitsToDevice exists in Windows 3.0 onward, so there is no Win32s
//     availability problem and no new import to resolve dynamically.
//   * The DIB is 24bpp regardless of the remote format.  GDI converts to the
//     display format (including dithering to a palette on an 8-bit display),
//     which is exactly what PALETTERGB + SetPixel was relying on before.
//   * DIB scanlines must be DWORD-aligned, and a bottom-up DIB stores the last
//     row first - both handled below.
//   * The buffer is reused across calls.  For a 4096-pixel-wide framebuffer a
//     full-width band is 12 KB per row; we cap the buffer and draw in bands so
//     that a large update does not need a large allocation.  Memory is the other
//     scarce resource on these machines.
//
//  If a DIB cannot be allocated, both functions fall back to the old per-pixel
//  loop so that drawing still works, just slowly.
//
// ==========================================================================

// Rows per SetDIBitsToDevice call.  16 rows x 4096 px x 3 bytes = 192 KB worst
// case; for a typical 1024-wide desktop it is 48 KB.
#define DIB_BAND_ROWS 16

bool ClientConnection::CheckDibBufferSize(size_t bufsize)
{
	if (m_dibbufsize >= bufsize && m_dibbuf != NULL)
		return true;

	if (bufsize > 0x00100000)		// 1 MB ceiling; we draw in bands
		return false;

	unsigned char *newbuf = new unsigned char[bufsize];
	if (newbuf == NULL)
		return false;

	if (m_dibbuf != NULL)
		delete [] m_dibbuf;
	m_dibbuf = newbuf;
	m_dibbufsize = bufsize;
	return true;
}

//
// DrawPixelBlock - draw w*h pixels of the remote format at (x,y).
//
// 'bpp' is the remote bits per pixel (8, 16 or 32); 'buffer' holds w*h pixels in
// that format, row by row, top row first.
//
void ClientConnection::DrawPixelBlock(char *buffer, int bpp, int x, int y,
									  int w, int h)
{
	if (buffer == NULL || w <= 0 || h <= 0)
		return;
	if (m_hBitmapDC == NULL || m_hBitmap == NULL)
		return;

	SETUP_COLOR_SHORTCUTS;

	// DWORD-aligned 24bpp scanline.
	int stride = ((w * 3) + 3) & ~3;
	int bandRows = DIB_BAND_ROWS;
	if (bandRows > h)
		bandRows = h;

	if (bpp == 24) {
		// Defensive: SetupPixelFormat promotes 24 to 32 so this should not
		// happen, but a server may send 24bpp anyway.  Convert 3-byte
		// pixels here rather than misreading them as 4-byte (which is what
		// produced blank/garbled Raw/Zlib rects).
		bpp = 32;
		// NOTE: true 3-byte wire data cannot be reinterpreted as 4-byte;
		// callers must only reach here with padded data.  Fall through to
		// the slow path which at least draws something instead of nothing.
		SETPIXELS(buffer, 32, x, y, w, h)
		return;
	}

	if (!CheckDibBufferSize((size_t)stride * (size_t)bandRows)) {
		// Fall back to the slow path rather than not drawing at all.
		switch (bpp) {
		case 8:
			SETPIXELS(buffer, 8, x, y, w, h)
			break;
		case 16:
			SETPIXELS(buffer, 16, x, y, w, h)
			break;
		case 32:
			SETPIXELS(buffer, 32, x, y, w, h)
			break;
		default:
			vnclog.Print(0, _T("DrawPixelBlock: unsupported bpp %d\n"), bpp);
			break;
		}
		return;
	}

	BITMAPINFOHEADER bih;
	memset(&bih, 0, sizeof(bih));
	bih.biSize        = sizeof(BITMAPINFOHEADER);
	bih.biWidth       = w;
	bih.biPlanes      = 1;
	bih.biBitCount    = 24;
	bih.biCompression = BI_RGB;

	int rowsDone = 0;
	while (rowsDone < h) {
		int rows = h - rowsDone;
		if (rows > bandRows)
			rows = bandRows;

		// Fill the DIB bottom-up: DIB row 0 is the LAST source row of the band.
		int r;
		for (r = 0; r < rows; r++) {
			int srcRow = rowsDone + r;
			unsigned char *dst = m_dibbuf + (size_t)(rows - 1 - r) * stride;
			int j;

			switch (bpp) {
			case 8:
				{
					CARD8 *p = ((CARD8 *)buffer) + (size_t)srcRow * w;
					for (j = 0; j < w; j++) {
						CARD8 pix = *p++;
						// Blue, green, red - DIB byte order.
						*dst++ = (unsigned char)((((pix >> bs) & bm) * 255) / bm);
						*dst++ = (unsigned char)((((pix >> gs) & gm) * 255) / gm);
						*dst++ = (unsigned char)((((pix >> rs) & rm) * 255) / rm);
					}
				}
				break;
			case 16:
				{
					CARD16 *p = ((CARD16 *)buffer) + (size_t)srcRow * w;
					for (j = 0; j < w; j++) {
						CARD16 pix = *p++;
						*dst++ = (unsigned char)((((pix >> bs) & bm) * 255) / bm);
						*dst++ = (unsigned char)((((pix >> gs) & gm) * 255) / gm);
						*dst++ = (unsigned char)((((pix >> rs) & rm) * 255) / rm);
					}
				}
				break;
			case 32:
				{
					CARD32 *p = ((CARD32 *)buffer) + (size_t)srcRow * w;
					for (j = 0; j < w; j++) {
						CARD32 pix = *p++;
						*dst++ = (unsigned char)((((pix >> bs) & bm) * 255) / bm);
						*dst++ = (unsigned char)((((pix >> gs) & gm) * 255) / gm);
						*dst++ = (unsigned char)((((pix >> rs) & rm) * 255) / rm);
					}
				}
				break;
			default:
				return;
			}
		}

		bih.biHeight    = rows;
		bih.biSizeImage = (DWORD)stride * (DWORD)rows;

 		// WIN32S: SetDIBitsToDevice to a memory DC always returns -1 here,
		// so each band goes through a throwaway DDB instead: CreateDIBitmap
		// (core Windows 3.0, CBM_INIT takes our bottom-up 24-bit DIB as-is)
		// then BitBlt it into place.  BitBlt is proven working (DoBlit and
		// CopyRect use it with no errors).  This also sidesteps the x-offset
		// problem a direct SetDIBits would have (it writes full scanlines).
		// TEMP-DIAG (Raw blank): report the first result and every
		// fallback, bounded so the log stays small.
		int y0 = y + rowsDone;
		int dibScans = 0;
		{
			HBITMAP hb = CreateDIBitmap(m_hBitmapDC, &bih, CBM_INIT,
										m_dibbuf, (BITMAPINFO *)&bih,
										DIB_RGB_COLORS);
			if (hb != NULL) {
				HDC sdc = CreateCompatibleDC(m_hBitmapDC);
				if (sdc != NULL) {
					HGDIOBJ old = SelectObject(sdc, hb);
					if (BitBlt(m_hBitmapDC, x, y0, w, rows,
							   sdc, 0, 0, SRCCOPY))
						dibScans = rows;
					SelectObject(sdc, old);
					DeleteDC(sdc);
				}
				DeleteObject(hb);
			}
		}
		{
			static int s_dibDiag = 0;
			static int s_dibFallbacks = 0;
			if (s_dibDiag < 3) {
				s_dibDiag++;
				vnclog.Print(0, _T("DIAG DrawPixelBlock %d: %dx%d at %d,%d bpp=%d scans=%d\n"),
							 s_dibDiag, w, rows, x, y + rowsDone, bpp, dibScans);
			}
			if (dibScans != rows && s_dibFallbacks < 5) {
				s_dibFallbacks++;
				vnclog.Print(0, _T("DIAG DrawPixelBlock fallback %d (slow path)\n"),
							 s_dibFallbacks);
			}
		}
 		if (dibScans != rows) {
			char *slowBuf = buffer + (size_t)rowsDone * w *
				((bpp == 8) ? 1 : (bpp == 16) ? 2 : 4);
			switch (bpp) {
			case 8:
				SETPIXELS(slowBuf, 8, x, y + rowsDone, w, rows)
				break;
			case 16:
				SETPIXELS(slowBuf, 16, x, y + rowsDone, w, rows)
				break;
			default:
				SETPIXELS(slowBuf, 32, x, y + rowsDone, w, rows)
				break;
			}
		}

		rowsDone += rows;
	}
}

//
// DrawColorRefBlock - draw w*h pixels that are already COLORREFs at (x,y).
//
// This is the SETPIXELS_NOCONV case, used by the Tight decoder after JPEG
// decompression.
//
void ClientConnection::DrawColorRefBlock(char *buffer, int x, int y, int w, int h)
{
	if (buffer == NULL || w <= 0 || h <= 0)
		return;
	if (m_hBitmapDC == NULL || m_hBitmap == NULL)
		return;

	int stride = ((w * 3) + 3) & ~3;
	int bandRows = DIB_BAND_ROWS;
	if (bandRows > h)
		bandRows = h;

	if (!CheckDibBufferSize((size_t)stride * (size_t)bandRows)) {
		SETPIXELS_NOCONV(buffer, x, y, w, h)
		return;
	}

	BITMAPINFOHEADER bih;
	memset(&bih, 0, sizeof(bih));
	bih.biSize        = sizeof(BITMAPINFOHEADER);
	bih.biWidth       = w;
	bih.biPlanes      = 1;
	bih.biBitCount    = 24;
	bih.biCompression = BI_RGB;

	int rowsDone = 0;
	while (rowsDone < h) {
		int rows = h - rowsDone;
		if (rows > bandRows)
			rows = bandRows;

		int r;
		for (r = 0; r < rows; r++) {
			int srcRow = rowsDone + r;
			unsigned char *dst = m_dibbuf + (size_t)(rows - 1 - r) * stride;
			CARD32 *p = ((CARD32 *)buffer) + (size_t)srcRow * w;
			int j;
			for (j = 0; j < w; j++) {
				COLORREF c = (COLORREF)(*p++);
				// A COLORREF is 0x00bbggrr; a DIB pixel is b,g,r.
				*dst++ = (unsigned char)((c >> 16) & 0xFF);	// blue
				*dst++ = (unsigned char)((c >> 8) & 0xFF);	// green
				*dst++ = (unsigned char)(c & 0xFF);			// red
			}
		}

		bih.biHeight    = rows;
		bih.biSizeImage = (DWORD)stride * (DWORD)rows;

		// WIN32S: SetDIBitsToDevice to a memory DC always returns -1 here,
		// so bands go through a throwaway DDB (CreateDIBitmap + BitBlt,
		// both proven working) exactly as in DrawPixelBlock above.
		{
			int dibScans = 0;
			HBITMAP hb = CreateDIBitmap(m_hBitmapDC, &bih, CBM_INIT,
										m_dibbuf, (BITMAPINFO *)&bih,
										DIB_RGB_COLORS);
			if (hb != NULL) {
				HDC sdc = CreateCompatibleDC(m_hBitmapDC);
				if (sdc != NULL) {
					HGDIOBJ old = SelectObject(sdc, hb);
					if (BitBlt(m_hBitmapDC, x, y + rowsDone, w, rows,
							   sdc, 0, 0, SRCCOPY))
						dibScans = rows;
					SelectObject(sdc, old);
					DeleteDC(sdc);
				}
				DeleteObject(hb);
			}
			if (dibScans != rows) {
				SETPIXELS_NOCONV(buffer + (size_t)rowsDone * w * 4,
								 x, y + rowsDone, w, rows)
			}
		}

		rowsDone += rows;
	}
}


// Makes sure netbuf is at least as big as the specified size.
// Note that netbuf itself may change as a result of this call.
// Throws an exception on failure.
void ClientConnection::CheckBufferSize(size_t bufsize)
{
	if (m_netbufsize > bufsize) return;

	// Don't try to allocate more than 2 gigabytes.
	if (bufsize >= 0x80000000) {
		vnclog.Print(1, _T("Requested buffer size is too big (%u bytes)\n"),
					 (unsigned int)bufsize);
		throw WarningException("Requested buffer size is too big.");
	}

	// Win32s runs on a machine that may only have a few megabytes free, and a
	// hostile or buggy server can ask for a very large rectangle.  Refuse
	// anything absurd early rather than thrashing or failing mid-decode.
	if (bufsize > 0x00800000) {		// 8 MB
		vnclog.Print(1, _T("Refusing oversized buffer request (%u bytes)\n"),
					 (unsigned int)bufsize);
		throw WarningException("Server requested an unreasonably large buffer.");
	}

	omni_mutex_lock l(m_bufferMutex);

	char *newbuf = new char[bufsize + 256];
	if (newbuf == NULL) {
		throw ErrorException("Insufficient memory to allocate network buffer.");
	}

	if (m_netbuf != NULL) {
		delete[] m_netbuf;
	}
	m_netbuf = newbuf;
	m_netbufsize = bufsize + 256;
	vnclog.Print(4, _T("Buffer size expanded to %u\n"),
				 (unsigned int)m_netbufsize);
}

// Makes sure zlibbuf is at least as big as the specified size.
// Note that zlibbuf itself may change as a result of this call.
// Throws an exception on failure.
void ClientConnection::CheckZlibBufferSize(size_t bufsize)
{
	if (m_zlibbufsize > bufsize) return;

	// Don't try to allocate more than 2 gigabytes.
	if (bufsize >= 0x80000000) {
		vnclog.Print(1, _T("Requested zlib buffer size is too big (%u bytes)\n"),
					 (unsigned int)bufsize);
		throw WarningException("Requested zlib buffer size is too big.");
	}

	// omni_mutex_lock l(m_bufferMutex);

	unsigned char *newbuf = new unsigned char[bufsize + 256];
	if (newbuf == NULL) {
		throw ErrorException("Insufficient memory to allocate zlib buffer.");
	}

	if (m_zlibbuf != NULL) {
		delete[] m_zlibbuf;
	}
	m_zlibbuf = newbuf;
	m_zlibbufsize = bufsize + 256;
	vnclog.Print(4, _T("Zlib buffer size expanded to %u\n"),
				 (unsigned int)m_zlibbufsize);
}

//
// Invalidate a screen rectangle respecting scaling set by user.
//

void ClientConnection::InvalidateScreenRect(const RECT *pRect) {
	RECT rect;

	if (m_hwnd == NULL || pRect == NULL)
		return;

	// If we're scaling, we transform the coordinates of the rectangle
	// received into the corresponding window coords, and invalidate
	// *that* region.

	if (m_opts.m_scaling) {
		// First, we adjust coords to avoid rounding down when scaling.
		int n = m_opts.m_scale_num;
		int d = m_opts.m_scale_den;
		// Divide-by-zero guard.  m_scale_den comes from the registry and from
		// the /scale command line switch; FixScaling() clamps it, but this is
		// on the hot path for every update and an integer divide by zero on
		// Win32s faults the whole VM, not just this task.
		if (d < 1) d = 1;
		if (n < 1) n = 1;
		int left   = (pRect->left / d) * d;
		int top    = (pRect->top  / d) * d;
		int right  = (pRect->right  + d - 1) / d * d; // round up
		int bottom = (pRect->bottom + d - 1) / d * d; // round up

		// Then we scale the rectangle, which should now give whole numbers.
		rect.left   = (left   * n / d) - m_hScrollPos;
		rect.top    = (top    * n / d) - m_vScrollPos;
		rect.right  = (right  * n / d) - m_hScrollPos;
		rect.bottom = (bottom * n / d) - m_vScrollPos;
	} else {
		rect.left   = pRect->left   - m_hScrollPos;
		rect.top    = pRect->top    - m_vScrollPos;
		rect.right  = pRect->right  - m_hScrollPos;
		rect.bottom = pRect->bottom - m_vScrollPos;
	}
	InvalidateRect(m_hwnd, &rect, FALSE);
}

//
// Processing NewFBSize pseudo-rectangle. Create new framebuffer of
// the size specified in pfburh->r.w and pfburh->r.h, and change the
// window size correspondingly.
//

void ClientConnection::ReadNewFBSize(rfbFramebufferUpdateRectHeader *pfburh)
{
	// Same sanity check as ReadServerInit: this reallocates the framebuffer
	// bitmap from server-supplied dimensions, and every rectangle validated
	// afterwards is checked against these values.
	if (pfburh->r.w == 0 || pfburh->r.h == 0 ||
		pfburh->r.w > 4096 || pfburh->r.h > 4096) {
		vnclog.Print(0, _T("Bad NewFBSize %d x %d from server\n"),
					 (int)pfburh->r.w, (int)pfburh->r.h);
		throw ErrorException("Protocol error: implausible new framebuffer size.");
	}

	m_si.framebufferWidth = pfburh->r.w;
	m_si.framebufferHeight = pfburh->r.h;

	// Any cursor state refers to the old bitmap and the old saved-area bitmap.
	SoftCursorFree();

	CreateLocalFramebuffer();

	SizeWindow(false);
	RealiseFullScreenMode(true);
}

