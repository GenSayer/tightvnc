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


#ifndef CLIENTCONNECTION_H__
#define CLIENTCONNECTION_H__

#pragma once

#include "stdhdrs.h"
#ifdef UNDER_CE
#include "omnithreadce.h"
#else
#include "omnithread.h"
#endif
#include "CapsContainer.h"
#include "VNCOptions.h"
#include "VNCviewerApp.h"
#include "KeyMap.h"
#include "ConnectingDialog.h"
#include "FileTransfer.h"
#include "Win32sApi.h"
#include "zlib/zlib.h"
extern "C" {
#include "libjpeg/jpeglib.h"
}

#define SETTINGS_KEY_NAME "Software\\ORL\\VNCviewer\\Settings"
#define MAX_HOST_NAME_LEN 250

#define ZLIBHEX_DECOMP_UNINITED (-1)

#define TIGHT_ZLIB_BUFFER_SIZE 512

class ClientConnection;
typedef void (ClientConnection:: *tightFilterFunc)(int);

//
// WIN32S SINGLE-THREADED NOTE
//
// ClientConnection used to derive from omni_thread and run its receive loop on
// a second thread (run_undetached).  Win32s has no threads, so the class is now
// a plain object driven from the application's idle loop:
//
//   Run()      - blocking connect + handshake, as before, then StartSession()
//                instead of start_undetached()
//   PumpIdle() - called repeatedly from the main message loop; if a whole
//                server message is available it reads and processes exactly
//                one message, then returns.  Never blocks.
//   IsDead()   - true once the session has ended; the app then deletes us
//                from a safe place (never from inside a window procedure).
//
class ClientConnection
{
public:
	ClientConnection(VNCviewerApp *pApp);
	ClientConnection(VNCviewerApp *pApp, SOCKET sock);
	ClientConnection(VNCviewerApp *pApp, LPTSTR host, int port);
	ClientConnection(VNCviewerApp *pApp, LPTSTR configFile);
	virtual ~ClientConnection();
	void Run();
	void KillThread();
	void CopyOptions(ClientConnection *source);

	// Options accessors.  m_opts is private, and the auth-retry logic in
	// VNCviewerApp32 now needs to copy the options *out* of a connection before
	// deleting it (so that only one ClientConnection exists at a time - see the
	// notes there).  VNCOptions::operator= takes a non-const reference, hence
	// the non-const accessor.
	VNCOptions& GetOptions() { return m_opts; }
	void SetOptions(VNCOptions &opts) { m_opts = opts; }

	int  LoadConnection(char *fname, bool sess);
	void UnloadConnection() { m_opts.m_configSpecified = false; }

	// ---- single-threaded session driver (see ClientConnection.cpp) --------

	// Send the first update request and mark the session live.  Replaces
	// omni_thread::start_undetached().
	void StartSession();

	// Non-blocking: service at most one pending server message.  Returns
	// true if it did some work (so the caller can keep pumping before it
	// goes back to sleep in GetMessage).
	bool PumpIdle();

	// True when the session has finished and this object may be deleted.
	bool IsDead() { return m_dead; }

	// Called by the window procedure on WM_DESTROY.  Marks the object for
	// deletion by the app's reaper instead of doing the old join().
	void OnWindowDestroyed();

	int m_port;
    TCHAR m_host[MAX_HOST_NAME_LEN];
	HWND m_hSess;

private:
	// Window procedures.
	//
	// Each CALLBACK is a thin wrapper (defined in ClientConnection.cpp) that
	//   * returns DefWindowProc immediately if the 'pseudo-this' in
	//     GWL_USERDATA is not set yet - messages such as WM_GETMINMAXINFO and
	//     WM_NCCREATE arrive *during* CreateWindow, before SetWindowLong has
	//     run, and the original code dereferenced NULL for them; and
	//   * catches every Exception, so nothing is ever thrown across the USER32
	//     stack frame that dispatched the message.  MSVC 4.1 has no unwind
	//     information for that frame, so a throw there corrupts the stack or
	//     terminates the process.
	// The real handlers are the *Impl functions.
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WndProc1(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK Proc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK ScrollProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);

	static LRESULT WndProcImpl(ClientConnection *_this, HWND hwnd, UINT iMsg,
							   WPARAM wParam, LPARAM lParam);
	static LRESULT WndProc1Impl(ClientConnection *_this, HWND hwnd, UINT iMsg,
								WPARAM wParam, LPARAM lParam);
	static LRESULT ScrollProcImpl(ClientConnection *_this, HWND hwnd, UINT iMsg,
								  WPARAM wParam, LPARAM lParam);
	void DoBlit();
	VNCviewerApp *m_pApp;
	ConnectingDialog *m_connDlg;

	bool m_enableFileTransfers;
	bool m_fileTransferDialogShown;
	friend class FileTransfer;
	FileTransfer *m_pFileTransfer;

	SOCKET m_sock;
 	// WIN32S re-entrancy guard: set around the recv()/send() loops in
 	// ReadExact/WriteExact.  This build is single-threaded and the mutexes
 	// below are no-ops, so if a write ever runs while a read is in flight
 	// (or vice versa) it can only be OS-level dispatch during a blocking
 	// Winsock call - which the VNCVIEW logs confirm happens
 	// (WSAEINPROGRESS on a PointerEvent write mid-update).  Writes during a
 	// read are deferred to m_pendingHead/Tail and flushed when the read
 	// completes; the remaining EINPROGRESS retries cover transient busies.
 	bool m_inReadExact, m_inWriteExact;
	// WIN32S deferred-write queue: on Win32s the OS dispatches window input
	// from inside a blocking recv(), so a key/mouse/update-request WriteExact
	// can run while a bulk ReadExact (e.g. a full-screen Raw rect) is still
	// in flight on the same socket.  The stack answers the re-entrant send
	// with WSAEINPROGRESS and, once retries exhaust, the session drops
	// (VNCVIEW1/2.LOG: "WriteExact during ReadExact" -> 10036 -> 10038).
	// While m_inReadExact is set, WriteExact() enqueues a copy here instead
	// of touching the socket; ReadExact() flushes FIFO on success.
	struct PendingWrite { char *data; int len; PendingWrite *next; };
	PendingWrite *m_pendingHead, *m_pendingTail;
	void FlushPendingWrites();
	void DiscardPendingWrites();
	bool m_serverInitiated;
	HWND m_hwnd, m_hbands, m_hwnd1, 
		 m_hToolbar, m_hwndscroll;
		
	void Init(VNCviewerApp *pApp);
	void InitCapabilities();
	void CreateDisplay();
	HWND CreateToolbar();
	void GetConnectDetails();
	void Connect();
	void SetSocketOptions();
	void NegotiateProtocolVersion();
	void PerformAuthentication();
	int ReadSecurityType();
	int SelectSecurityType();
	void SetupTunneling();
	void PerformAuthenticationTight();
	void Authenticate(CARD32 authScheme);
	bool AuthenticateNone(char *errBuf, int errBufSize);
	bool AuthenticateVNC(char *errBuf, int errBufSize);
	void ReadServerInit();
	void ReadInteractionCaps();
	void ReadCapabilityList(CapsContainer *caps, int count);
	void SendClientInit();
	void CreateLocalFramebuffer();
	void SaveConnection();
	void SaveConnectionHistory();
	
	void SetupPixelFormat();
	void SetFormatAndEncodings();
	void SendSetPixelFormat(rfbPixelFormat newFormat);

	void SendIncrementalFramebufferUpdateRequest();
	void SendFullFramebufferUpdateRequest();
	void SendAppropriateFramebufferUpdateRequest();
	void SendFramebufferUpdateRequest(int x, int y, int w, int h, bool incremental);
	
	void ProcessPointerEvent(int x, int y, DWORD keyflags, UINT msg);
 	void SubProcessPointerEvent(int x, int y, DWORD keyflags);
	void SendPointerEvent(int x, int y, int buttonMask);
    void ProcessKeyEvent(int virtkey, DWORD keyData);
	void SendKeyEvent(CARD32 key, bool down);
	void SwitchOffKey();

	void ReadScreenUpdate();
	void Update(RECT *pRect);
	void SizeWindow(bool centered);
	void PositionChildWindow();
	bool ScrollScreen(int dx, int dy);
	void UpdateScrollbars();
	void EnableFullControlOptions();
	void EnableAction(int id, bool enable);

	void InvalidateScreenRect(const RECT *pRect);

	void ReadRawRect(rfbFramebufferUpdateRectHeader *pfburh);
	void ReadCopyRect(rfbFramebufferUpdateRectHeader *pfburh);
    void ReadRRERect(rfbFramebufferUpdateRectHeader *pfburh);
	void ReadCoRRERect(rfbFramebufferUpdateRectHeader *pfburh);
	void ReadHextileRect(rfbFramebufferUpdateRectHeader *pfburh);
	void HandleHextileEncoding8(int x, int y, int w, int h);
	void HandleHextileEncoding16(int x, int y, int w, int h);
	void HandleHextileEncoding32(int x, int y, int w, int h);
	void ReadZlibRect(rfbFramebufferUpdateRectHeader *pfburh);

	void ReadNewFBSize(rfbFramebufferUpdateRectHeader *pfburh);

	// ClientConnectionTight.cpp
	void ReadTightRect(rfbFramebufferUpdateRectHeader *pfburh);
	int ReadCompactLen();
	int InitFilterCopy (int rw, int rh);
	int InitFilterGradient (int rw, int rh);
	int InitFilterPalette (int rw, int rh);
	void FilterCopy8 (int numRows);
	void FilterCopy16 (int numRows);
	void FilterCopy24 (int numRows);
	void FilterCopy32 (int numRows);
	void FilterGradient8 (int numRows);
	void FilterGradient16 (int numRows);
	void FilterGradient24 (int numRows);
	void FilterGradient32 (int numRows);
	void FilterPalette (int numRows);
	void DecompressJpegRect(int x, int y, int w, int h);

	// ClientConnectionZlibHex.cpp
	void HandleZlibHexEncoding8(int x, int y, int w, int h);
	void HandleZlibHexEncoding16(int x, int y, int w, int h);
	void HandleZlibHexEncoding32(int x, int y, int w, int h);
	void HandleZlibHexSubencodingStream8(int x, int y, int w, int h, int subencoding);
	void HandleZlibHexSubencodingStream16(int x, int y, int w, int h, int subencoding);
	void HandleZlibHexSubencodingStream32(int x, int y, int w, int h, int subencoding);
	void HandleZlibHexSubencodingBuf8(int x, int y, int w, int h, int subencoding, unsigned char * buffer);
	void HandleZlibHexSubencodingBuf16(int x, int y, int w, int h, int subencoding, unsigned char * buffer);
	void HandleZlibHexSubencodingBuf32(int x, int y, int w, int h, int subencoding, unsigned char * buffer);
	void ReadZlibHexRect(rfbFramebufferUpdateRectHeader *pfburh);

	bool zlibDecompress(unsigned char *from_buf, unsigned char *to_buf, unsigned int count, unsigned int size, z_stream *decompressor);

	void ReadRBSRect(rfbFramebufferUpdateRectHeader *pfburh);
	BOOL DrawRBSRect8(int x, int y, int w, int h, CARD8 **pptr);
	BOOL DrawRBSRect16(int x, int y, int w, int h, CARD8 **pptr);
	BOOL DrawRBSRect32(int x, int y, int w, int h, CARD8 **pptr);

	// ClientConnectionCursor.cpp
	bool prevCursorSet;
	HDC m_hSavedAreaDC;
	HBITMAP m_hSavedAreaBitmap;
	COLORREF *rcSource;
	bool *rcMask;
	int rcHotX, rcHotY, rcWidth, rcHeight;
	int rcCursorX, rcCursorY;
	int rcLockX, rcLockY, rcLockWidth, rcLockHeight;
	bool rcCursorHidden, rcLockSet;

	void ReadCursorShape(rfbFramebufferUpdateRectHeader *pfburh);
	void ReadCursorPos(rfbFramebufferUpdateRectHeader *pfburh);
	void SoftCursorLockArea(int x, int y, int w, int h);
	void SoftCursorUnlockScreen();
	void SoftCursorMove(int x, int y);
	void SoftCursorFree();
	bool SoftCursorInLockedArea();
	void SoftCursorSaveArea();
	void SoftCursorRestoreArea();
	void SoftCursorDraw();
	void SoftCursorToScreen(RECT *screenArea, POINT *cursorOffset);

	// ClientConnectionFullScreen.cpp
	void SetFullScreenMode(bool enable);
	bool InFullScreenMode();
	void RealiseFullScreenMode(bool suppressPrompt);
	bool BumpScroll(int x, int y);

	// ClientConnectionClipboard.cpp
	void ProcessLocalClipboardChange();
	void UpdateLocalClipboard(char *buf, size_t len);
	void SendClientCutText(char *str, size_t len);
	void ReadServerCutText();

	void ReadSetColourMapEntries();
	void ReadBell();

	void SendRFBMsg(CARD8 msgType, void* data, int length);
	void ReadExact(char *buf, int bytes);
	void ReadString(char *buf, int length);
	void WriteExact(char *buf, int bytes);
	char *ReadFailureReason();

	// Session state.
	//
	// m_bKillThread keeps its old name (it is tested in many places) but now
	// simply means "stop servicing this session".
	// m_dead is set when the session is over and the object can be freed.
	bool m_bKillThread;
	bool m_dead;

	// True once StartSession() has run, so PumpIdle() knows the handshake is
	// complete and it is legal to read protocol messages.
	bool m_sessionStarted;

	// Non-blocking check: is there at least one byte to read on the socket?
	bool SocketHasData();

	// ---- bulk pixel drawing (see the long comment in ClientConnection.cpp) --
	//
	// These replace the per-pixel SetPixel loops that SETPIXELS/SETPIXELS_NOCONV
	// used to expand to.  One SetDIBitsToDevice call per band of rows instead of
	// one GDI call per pixel.
	void DrawPixelBlock(char *buffer, int bpp, int x, int y, int w, int h);
	void DrawColorRefBlock(char *buffer, int x, int y, int w, int h);

	// Scratch DIB used by the two functions above.  Reused between calls; freed
	// in the destructor.
	unsigned char *m_dibbuf;
	size_t m_dibbufsize;
	bool CheckDibBufferSize(size_t bufsize);

	// Utilities

	// These draw a solid rectangle of colour on the bitmap
	// They assume the bitmap is already selected into the DC, and the
	// DC is locked if necessary.
#ifndef UNDER_CE
	// Normally this is an inline call to a GDI method.
	inline void FillSolidRect(RECT *pRect, COLORREF color) {
		COLORREF oldbgcol = SetBkColor(m_hBitmapDC, color);
		// This is the call MFC uses for FillSolidRect. Who am I to argue?
		::ExtTextOut(m_hBitmapDC, 0, 0, ETO_OPAQUE, pRect, NULL, 0, NULL);			
	};
#else
	// Under WinCE this is a manual insert into a pixmap, 
	// and is a little too complicated for an inline.
	void FillSolidRect(RECT *pRect, COLORREF color);
#endif // UNDER_CE

	inline void FillSolidRect(int x, int y, int w, int h, COLORREF color) {
		RECT r;
		r.left = x;		r.right = x + w;
		r.top = y;		r.bottom = y + h;
		FillSolidRect(&r, color);
	};

    // how many other windows are owned by this process?
    unsigned int CountProcessOtherWindows();

    // Buffer for network operations
	void CheckBufferSize(size_t bufsize);
	char *m_netbuf;
	size_t m_netbufsize;
	// These are now empty no-op objects (see omnithread/omnithread.h).  They
	// are retained only so that the many "omni_mutex_lock l(m_xxxMutex);"
	// statements in the decoders compile without edits.
	omni_mutex m_bufferMutex, 
		m_bitmapdcMutex,  m_clipMutex,
		m_readMutex, m_writeMutex, m_sockMutex,
		m_cursorMutex;

	// Buffer for zlib decompression.
	void CheckZlibBufferSize(size_t bufsize);
	unsigned char *m_zlibbuf;
	size_t m_zlibbufsize;

	// zlib decompression state
	bool m_decompStreamInited;
	z_stream m_decompStream;
	z_stream m_decompStreamRaw;
	z_stream m_decompStreamEncoded;

	// Variables used by tight encoding:

	// Separate buffer for tight-compressed data.
	char m_tightbuf[TIGHT_ZLIB_BUFFER_SIZE];

	// Four independent compression streams for zlib library.
	z_stream m_tightZlibStream[4];
	bool m_tightZlibStreamActive[4];

	// Tight filter stuff. Should be initialized by filter initialization code.
	tightFilterFunc m_tightCurrentFilter;
	bool m_tightCutZeros;
	int m_tightRectWidth, m_tightRectColors;
	COLORREF m_tightPalette[256];
	CARD8 m_tightPrevRow[2048*3*sizeof(CARD16)];

	// Bitmap for local copy of screen, and DC for writing to it.
	HBITMAP m_hBitmap;
	HDC		m_hBitmapDC;
	HPALETTE m_hPalette;

#ifdef UNDER_CE
	// Under WinCE this points to the DIB pixels.
	BYTE* m_bits;
#endif
 
	// Keyboard mapper
	KeyMap m_keymap;

	// RFB settings
	VNCOptions m_opts;

	// Protocol capabilities
	CapsContainer m_tunnelCaps;		// known tunneling/encryption methods
	CapsContainer m_authCaps;		// known authentication schemes
	CapsContainer m_serverMsgCaps;	// known non-standard server messages
	CapsContainer m_clientMsgCaps;	// known non-standard client messages
	CapsContainer m_encodingCaps;	// known encodings besides Raw

	TCHAR *m_desktopName;
	unsigned char m_encPasswd[8];
	bool m_passwdSet;
	int m_authScheme;
	rfbServerInitMsg m_si;
	rfbPixelFormat m_myFormat, m_pendingFormat;
	// protocol version in use.
	int m_minorVersion;
	bool m_tightVncProtocol;
	bool m_threadStarted, m_running;
	// mid-connection format change requested
	bool m_pendingFormatChange;
	// Display connection info;
	void ShowConnInfo();

	// Window may be scrollable - these control the scroll position
	int m_hScrollPos, m_hScrollMax, m_vScrollPos, m_vScrollMax;

	// The size of the current client area
	int m_cliwidth, m_cliheight;
	// The size of a window needed to hold entire screen without scrollbars
	int m_fullwinwidth, m_fullwinheight;
	int m_winwidth, m_winheight;	
	// The size of the CE CommandBar
	int m_barheight;

	// Dormant basically means minimized; updates will not be requested 
	// while dormant.
	void SetDormant(bool newstate);
	bool m_dormant;

	// The number of bytes required to hold at least one pixel.
	unsigned int m_minPixelBytes;
	// Next window in clipboard chain
	HWND m_hwndNextViewer; 
	bool m_initialClipboardSeen;		

	// Are we waiting on a timer for emulating three buttons?
	bool m_waitingOnEmulateTimer;
	// Or are we emulating the middle button now?
	bool m_emulatingMiddleButton;
	// Emulate 3 buttons mouse timer:
	UINT m_emulate3ButtonsTimer;
	// Buttons pressed, waiting for timer in emulating 3 buttons:
	DWORD m_emulateKeyFlags;
	int m_emulateButtonPressedX;
	int m_emulateButtonPressedY;

};

// Some handy classes for temporary GDI object selection
// These select objects when constructed and automatically release them when destructed.
//
// Both guard against a NULL DC or object.  m_hPalette is NULL on a true-colour
// display (SetupPixelFormat only creates a palette for 8-bit), and m_hBitmapDC
// is NULL until CreateDisplay has run - so these constructors were being handed
// NULLs on ordinary code paths.
class ObjectSelector {
public:
	ObjectSelector(HDC hdc, HGDIOBJ hobj) {
		m_hdc = hdc;
		m_hOldObj = NULL;
		if (hdc != NULL && hobj != NULL)
			m_hOldObj = SelectObject(hdc, hobj);
	}
	~ObjectSelector() {
		if (m_hdc != NULL && m_hOldObj != NULL)
			m_hOldObj = SelectObject(m_hdc, m_hOldObj);
	}
	HGDIOBJ m_hOldObj;
	HDC m_hdc;
};

class PaletteSelector {
public:
	PaletteSelector(HDC hdc, HPALETTE hpal) { 
		m_hdc = hdc; 
		m_hOldPal = NULL;
		if (hdc != NULL && hpal != NULL) {
			m_hOldPal = SelectPalette(hdc, hpal, FALSE); 
			RealizePalette(hdc);
		}
	}
	~PaletteSelector() { 
		if (m_hdc != NULL && m_hOldPal != NULL) {
			m_hOldPal = SelectPalette(m_hdc, m_hOldPal, FALSE); 
			RealizePalette(m_hdc);
		}
	}
	HPALETTE m_hOldPal;
	HDC m_hdc;
};

class TempDC {
public:
	TempDC(HWND hwnd) { m_hdc = GetDC(hwnd); m_hwnd = hwnd; }
	// Do not release a DC we never got.  ReleaseDC(hwnd, NULL) is a bad-handle
	// call, and GetDC does fail on Win32s once the (small, fixed) common DC
	// cache is exhausted - which is exactly what happens if a DC is leaked
	// anywhere in the paint path.
	~TempDC() { if (m_hdc != NULL) ReleaseDC(m_hwnd, m_hdc); }
	operator HDC() {return m_hdc;};
	HDC m_hdc;
	HWND m_hwnd;
};

// Colour decoding utility functions
// Define rs,rm, bs,bm, gs & gm before using, eg with the following:

#define SETUP_COLOR_SHORTCUTS \
	 CARD8 rs = m_myFormat.redShift;   CARD16 rm = m_myFormat.redMax;   \
     CARD8 gs = m_myFormat.greenShift; CARD16 gm = m_myFormat.greenMax; \
     CARD8 bs = m_myFormat.blueShift;  CARD16 bm = m_myFormat.blueMax;  \

// read a pixel from the given address, and return a color value
#define COLOR_FROM_PIXEL8_ADDRESS(p) (PALETTERGB( \
                (int) (((*(CARD8 *)(p) >> rs) & rm) * 255 / rm), \
                (int) (((*(CARD8 *)(p) >> gs) & gm) * 255 / gm), \
                (int) (((*(CARD8 *)(p) >> bs) & bm) * 255 / bm) ))

#define COLOR_FROM_PIXEL16_ADDRESS(p) (PALETTERGB( \
                (int) ((( *(CARD16 *)(p) >> rs) & rm) * 255 / rm), \
                (int) ((( *(CARD16 *)(p) >> gs) & gm) * 255 / gm), \
                (int) ((( *(CARD16 *)(p) >> bs) & bm) * 255 / bm) ))

#define COLOR_FROM_PIXEL24_ADDRESS(p) (PALETTERGB( \
                (int) (((CARD8 *)(p))[0]), \
                (int) (((CARD8 *)(p))[1]), \
                (int) (((CARD8 *)(p))[2]) ))

#define COLOR_FROM_PIXEL32_ADDRESS(p) (PALETTERGB( \
                (int) ((( *(CARD32 *)(p) >> rs) & rm) * 255 / rm), \
                (int) ((( *(CARD32 *)(p) >> gs) & gm) * 255 / gm), \
                (int) ((( *(CARD32 *)(p) >> bs) & bm) * 255 / bm) ))

// The following may be faster if you already have a pixel value of the appropriate size
#define COLOR_FROM_PIXEL8(p) (PALETTERGB( \
                (int) (((p >> rs) & rm) * 255 / rm), \
                (int) (((p >> gs) & gm) * 255 / gm), \
                (int) (((p >> bs) & bm) * 255 / bm) ))

#define COLOR_FROM_PIXEL16(p) (PALETTERGB( \
                (int) ((( p >> rs) & rm) * 255 / rm), \
                (int) ((( p >> gs) & gm) * 255 / gm), \
                (int) ((( p >> bs) & bm) * 255 / bm) ))

#define COLOR_FROM_PIXEL32(p) (PALETTERGB( \
                (int) (((p >> rs) & rm) * 255 / rm), \
                (int) (((p >> gs) & gm) * 255 / gm), \
                (int) (((p >> bs) & bm) * 255 / bm) ))


#ifdef UNDER_CE
#define SETPIXEL(b,x,y,c) SetPixel((b),(x),(y),(c))
#else
// SetPixelV is Win95+ and is NOT exported by the Win32s GDI32 stub, so it must
// never appear in the import table.  Win32sSetPixelV (Win32sApi.h) resolves it
// at run time and falls back to SetPixel.
#define SETPIXEL(b,x,y,c) Win32sSetPixelV((b),(x),(y),(c))
#endif

#define SETPIXELS(buffer, bpp, x, y, w, h)										\
	{																			\
		CARD##bpp *p = (CARD##bpp *) buffer;									\
        register CARD##bpp pix;													\
		for (int k = y; k < y+h; k++) {											\
			for (int j = x; j < x+w; j++) {										\
                    pix = *p;													\
                    SETPIXEL(m_hBitmapDC, j,k, COLOR_FROM_PIXEL##bpp##(pix));	\
					p++;														\
			}																	\
		}																		\
	}

#define SETPIXELS_NOCONV(buffer, x, y, w, h)									\
	{																			\
		CARD32 *p = (CARD32 *) buffer;											\
		for (int k = y; k < y+h; k++) {											\
			for (int j = x; j < x+w; j++) {										\
                    SETPIXEL(m_hBitmapDC, j,k, *p);	                            \
					p++;														\
			}																	\
		}																		\
	}

#endif // CLIENTCONNECTION_H__

